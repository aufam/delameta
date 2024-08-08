#ifndef PROJECT_DELAMETA_MODBUS_SERVER_H
#define PROJECT_DELAMETA_MODBUS_SERVER_H

#include "delameta/modbus/api.h"
#include "delameta/stream.h"

namespace Project::delameta::modbus {

    class Server : public Movable {
    public:
        Server(uint8_t server_address) : server_address(server_address) {}
        virtual ~Server() = default;

        Server(Server&&) noexcept = default;
        Server& operator=(Server&&) noexcept = default;

        const std::function<bool()>& CoilGetter(uint16_t register_address, std::function<bool()> getter);
        const std::function<void(bool)>& CoilSetter(uint16_t register_address, std::function<void(bool)> setter);

        const std::function<uint16_t()>& HoldingRegisterGetter(uint16_t register_address, std::function<uint16_t()> getter);
        const std::function<void(uint16_t)>& HoldingRegisterSetter(uint16_t register_address, std::function<void(uint16_t)> setter);

        const std::function<bool()>& DiscreteInputGetter(uint16_t register_address, std::function<bool()> getter);
        const std::function<uint16_t()>& AnalogInputGetter(uint16_t register_address, std::function<uint16_t()> getter);

        const std::function<uint8_t()>& ExceptionStatusGetter(std::function<uint8_t()> getter);
        const std::function<Result<uint16_t>(uint16_t)>& DiagnosticGetter(uint8_t sub_function, std::function<Result<uint16_t>(uint16_t)> getter);

        uint8_t server_address;

        std::unordered_map<uint16_t, std::function<bool()>> coil_getters;
        std::unordered_map<uint16_t, std::function<void(bool)>> coil_setters;

        std::unordered_map<uint16_t, std::function<uint16_t()>> holding_register_getters;
        std::unordered_map<uint16_t, std::function<void(uint16_t)>> holding_register_setters;

        std::unordered_map<uint16_t, std::function<bool()>> discrete_input_getters;
        std::unordered_map<uint16_t, std::function<uint16_t()>> analog_input_getters;

        std::function<uint8_t()> exception_status_getter;
        std::unordered_map<uint16_t, std::function<Result<uint16_t>(uint16_t)>> diagnostic_getters;

        std::function<void(const std::string&, const std::vector<uint8_t>&, const std::vector<uint8_t>&)> logger;

        void bind(StreamSessionServer&);
        Result<std::vector<uint8_t>> execute(const std::vector<uint8_t>& data) const;

    protected:
        Result<std::vector<uint8_t>> execute_read_coils(const std::vector<uint8_t>& data) const;
        Result<std::vector<uint8_t>> execute_read_discrete_inputs(const std::vector<uint8_t>& data) const;

        Result<std::vector<uint8_t>> execute_read_holding_registers(const std::vector<uint8_t>& data) const;
        Result<std::vector<uint8_t>> execute_read_input_registers(const std::vector<uint8_t>& data) const;
        
        Result<std::vector<uint8_t>> execute_write_single_coil(const std::vector<uint8_t>& data) const;
        Result<std::vector<uint8_t>> execute_write_single_register(const std::vector<uint8_t>& data) const;
        
        Result<std::vector<uint8_t>> execute_write_multiple_coils(const std::vector<uint8_t>& data) const;
        Result<std::vector<uint8_t>> execute_write_multiple_registers(const std::vector<uint8_t>& data) const;
        
        Result<std::vector<uint8_t>> execute_read_exception_status(const std::vector<uint8_t>& data) const;
        Result<std::vector<uint8_t>> execute_diagnostic(const std::vector<uint8_t>& data) const;
    };
}

#endif