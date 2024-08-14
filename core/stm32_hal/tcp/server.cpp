#include "delameta/tcp/server.h"
#include "delameta/debug.h"
#include "etl/time.h"
#include "socket.h"

using namespace Project;
using namespace Project::delameta;
using namespace etl::literals;

using etl::Err;
using etl::Ok;

auto tcp::Server::New(const char* file, int line, Args args) -> Result<Server> {
    return Socket::New(file, line, Sn_MR_TCP, args.port, Sn_MR_ND).then([&](Socket socket) {
        return Server(std::move(socket), args.port);
    });
}

tcp::Server::Server(Socket&& socket, int port) 
    : StreamSessionServer({})
    , socket(std::move(socket)) 
    , port(port) {}

tcp::Server::Server(Server&& other) 
    : StreamSessionServer(std::move(other.handler))
    , socket(std::move(other.socket))
    , port(other.port)
    , on_stop(std::move(other.on_stop)) {}

tcp::Server::~Server() {
    stop();
}

auto tcp::Server::start() -> Result<void> {
    bool is_running = true;

    while (is_running) {
        ::listen(socket.socket);

        while (getSn_SR(socket.socket) != SOCK_ESTABLISHED) {
            etl::time::sleep(1ms);
        }

        for (int cnt = 1; is_running; ++cnt) {
            auto read_result = socket.read();
            if (read_result.is_err()) {
                warning(socket.file, socket.line, read_result.unwrap_err().what);
                break;
            }

            auto stream = execute_stream_session(socket, "", read_result.unwrap());
            stream >> socket;

            if (not socket.keep_alive) {
                if (socket.max > 0 and cnt >= socket.max) {
                    info(socket.file, socket.line, "Reached maximum receive: " + std::to_string(socket.socket));
                }
                break;
                info(socket.file, socket.line, "not keep alive");
            }
        }

        ::disconnect(socket.socket);
        etl::time::sleep(1ms);
        auto res = ::socket(socket.socket, Sn_MR_TCP, port, Sn_MR_ND);
        if (res < 0) {
            warning(socket.file, socket.line, "Unable to initialize socket again");
            break;
        }
        etl::time::sleep(1ms);
    }

    stop();
    return etl::Ok();
}

void tcp::Server::stop() {
    if (on_stop) {
        on_stop();
    }
}