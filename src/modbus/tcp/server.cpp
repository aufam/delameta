#include "delameta/modbus/tcp/server.h"
#include "../../delameta.h"

using namespace Project;
using namespace Project::delameta;

auto modbus::tcp::Server::New(const char* file, int line, Args args) -> delameta::Result<Server> {
    return delameta::tcp::Server::New(file, line, args).then([](delameta::tcp::Server server) {
        return modbus::tcp::Server(std::move(server));
    });
}

modbus::tcp::Server::Server(delameta::tcp::Server&& other) 
    : delameta::tcp::Server(std::move(other)) 
    , modbus::Server(0xff) {}

Stream modbus::tcp::Server::execute_stream_session(Socket& socket, const std::string& client_ip, const std::vector<uint8_t>& data) {
    auto res = execute(data);
    Stream s;
    if (res.is_ok()) {
        if (logger) logger(client_ip, data, res.unwrap());
        s << std::move(res.unwrap());
    } else {
        warning(socket.file, socket.line, res.unwrap_err().what);
    }
    return s;
}