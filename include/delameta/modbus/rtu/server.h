#ifndef PROJECT_DELAMETA_MODBUS_TCP_SERVER_H
#define PROJECT_DELAMETA_MODBUS_TCP_SERVER_H

#include "delameta/modbus/api.h"
#include "delameta/serial/server.h"

namespace Project::delameta::modbus::rtu {

    class Server : public delameta::serial::Server, public delameta::modbus::Server {
    public:
        Server(Server&&) = default;
        Server& operator=(Server&&) = default;
        virtual ~Server() = default;

        struct Args {
            uint8_t server_address;
            std::string port; 
            int baud; 
        };

        static delameta::Result<Server> New(const char* file, int line, Args args);
        
        std::function<void(const std::vector<uint8_t>&, const std::vector<uint8_t>&)> logger = {};

    protected:
        Server(delameta::serial::Server&&, uint8_t server_address);

        Stream execute_stream_session(FileDescriptor& fd, const std::vector<uint8_t>& data) override;
    };
}

#endif