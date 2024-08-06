#ifndef PROJECT_DELAMETA_MODBUS_TCP_CLIENT_H
#define PROJECT_DELAMETA_MODBUS_TCP_CLIENT_H

#include "delameta/modbus/api.h"
#include "delameta/serial/client.h"

namespace Project::delameta::modbus::rtu {
    
    class Client : public delameta::serial::Client, public delameta::modbus::Client {
    public:
        Client(Client&&) = default;
        Client& operator=(Client&&) = default;
        virtual ~Client() = default;

        struct Args {
            int server_address;
            std::string port; 
            int baud; 
            int timeout = 5;
        };

        static delameta::Result<Client> New(const char* file, int line, Args args);

    protected:
        Client(delameta::serial::Client&&, uint8_t server_address);
        modbus::Result<std::vector<uint8_t>> request(std::vector<uint8_t> data) override;
    };
}

#endif
