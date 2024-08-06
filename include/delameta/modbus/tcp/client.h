#ifndef PROJECT_DELAMETA_MODBUS_TCP_CLIENT_H
#define PROJECT_DELAMETA_MODBUS_TCP_CLIENT_H

#include "delameta/modbus/api.h"
#include "delameta/tcp/client.h"

namespace Project::delameta::modbus::tcp {
    
    class Client : public delameta::tcp::Client, public delameta::modbus::Client {
    public:
        Client(Client&&) = default;
        Client& operator=(Client&&) = default;
        virtual ~Client() = default;

        static delameta::Result<Client> New(const char* file, int line, Args args);

    protected:
        Client(delameta::tcp::Client&&);
        modbus::Result<std::vector<uint8_t>> request(std::vector<uint8_t> data) override;
    };
}

#endif
