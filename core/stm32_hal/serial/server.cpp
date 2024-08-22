#include "delameta/serial/server.h"
#include "delameta/debug.h"
#include "etl/time.h"

using namespace Project;
using namespace Project::delameta;
using namespace etl::literals;

using etl::Err;
using etl::Ok;

auto serial::Server::New(const char* file, int line, Args args) -> Result<Server> {
    return FileDescriptor::Open(file, line, args.port.c_str(), args.baud).then([&](FileDescriptor serial) {
        serial.timeout = -1;
        return Server(std::move(serial));
    });
}

serial::Server::Server(FileDescriptor&& fd) 
    : StreamSessionServer({})
    , fd(std::move(fd)) {}

serial::Server::Server(Server&& other) 
    : StreamSessionServer(std::move(other.handler))
    , fd(std::move(other.fd))
    , on_stop(std::move(other.on_stop)) {}

serial::Server::~Server() {
    stop();
}

auto serial::Server::start() -> Result<void> {
    bool is_running = true;
    on_stop = [this, &is_running]() {
        is_running = false;
        on_stop = {};
    };
 
    while (is_running) {
        fd.wait_until_ready();
        auto read_result = fd.read();
        if (read_result.is_err()) {
            warning(fd.file, fd.line, read_result.unwrap_err().what);
            continue;
        }

        auto stream = execute_stream_session(fd, "", read_result.unwrap());
        stream >> fd;
    }

    stop();
    return etl::Ok();
}

void serial::Server::stop() {
    if (on_stop) {
        on_stop();
    }
}