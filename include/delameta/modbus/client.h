#ifndef PROJECT_DELAMETA_MODBUS_API_CLIENT_H
#define PROJECT_DELAMETA_MODBUS_API_CLIENT_H

#include "delameta/modbus/api.h"
#include "delameta/stream.h"

namespace Project::delameta::modbus {

    class Client {
    public:
        Client(uint8_t server_address, StreamSessionClient& session) : server_address(server_address), session(session) {}
        virtual ~Client() = default;

        Result<std::vector<uint8_t>> request(std::vector<uint8_t> data);

        Result<std::vector<bool>> ReadCoils(uint16_t register_address, uint16_t n_register);
        Result<std::vector<bool>> ReadDiscreteInputs(uint16_t register_address, uint16_t n_register);

        Result<std::vector<uint16_t>> ReadHoldingRegisters(uint16_t register_address, uint16_t n_register);
        Result<std::vector<uint16_t>> ReadInputRegisters(uint16_t register_address, uint16_t n_register);

        Result<void> WriteSingleCoil(uint16_t register_address, bool value);
        Result<void> WriteSingleRegister(uint16_t register_address, uint16_t value);

        Result<void> WriteMultipleCoils(uint16_t register_address, const std::vector<bool>& values);
        Result<void> WriteMultipleRegisters(uint16_t register_address, const std::vector<uint16_t>& values);
    
        Result<uint8_t> ReadExceptionStatus();
        Result<uint16_t> Diagnostic(uint16_t sub_function, uint16_t input);

        uint8_t server_address;
        StreamSessionClient& session;
    };
}

#endif