#include "delameta/modbus/rtu/server.h"
#include "../../delameta.h"

using namespace Project;
using namespace Project::delameta;

auto modbus::rtu::Server::New(const char* file, int line, Args args) -> delameta::Result<Server> {
    return delameta::serial::Server::New(file, line, {args.port, args.baud})
    .then([addr=args.server_address](delameta::serial::Server server) {
        return modbus::rtu::Server(std::move(server), addr);
    });
}

modbus::rtu::Server::Server(delameta::serial::Server&& other, uint8_t server_address) 
    : delameta::serial::Server(std::move(other)) 
    , modbus::Server(server_address) {}

Stream modbus::rtu::Server::execute_stream_session(FileDescriptor& fd, const std::vector<uint8_t>& data) {
    auto res = execute(data);
    Stream s;
    if (res.is_ok()) {
        if (logger) logger(data, res.unwrap());
        s << std::move(res.unwrap());
    } else {
        warning(fd.file, fd.line, res.unwrap_err().what);
    }
    return s;
}