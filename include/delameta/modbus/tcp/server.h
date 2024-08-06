#ifndef PROJECT_DELAMETA_MODBUS_TCP_SERVER_H
#define PROJECT_DELAMETA_MODBUS_TCP_SERVER_H

#include "delameta/modbus/api.h"
#include "delameta/tcp/server.h"

namespace Project::delameta::modbus::tcp {

    class Server : public delameta::tcp::Server, public delameta::modbus::Server {
    public:
        Server(Server&&) = default;
        Server& operator=(Server&&) = default;
        virtual ~Server() = default;

        static delameta::Result<Server> New(const char* file, int line, Args args);
        
        std::function<void(const std::string&, const std::vector<uint8_t>&, const std::vector<uint8_t>&)> logger = {};

    protected:
        Server(delameta::tcp::Server&&);

        Stream execute_stream_session(Socket& socket, const std::string& client_ip, const std::vector<uint8_t>& data) override;
    };
}

#endif