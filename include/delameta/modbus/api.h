#ifndef PROJECT_DELAMETA_MODBUS_API_H
#define PROJECT_DELAMETA_MODBUS_API_H

#include "delameta/error.h"
#include <vector>
#include <functional>
#include <unordered_map>

namespace Project::delameta::modbus {
    enum FunctionCode {
        FunctionCodeReadCoils = 1,
        FunctionCodeReadDiscreteInputs = 2,
        FunctionCodeReadHoldingRegisters = 3,
        FunctionCodeReadInputRegisters = 4,
        FunctionCodeWriteSingleCoil = 5,
        FunctionCodeWriteSingleRegister = 6,
        FunctionCodeReadExceptionStatus = 7,
        FunctionCodeDiagnostic = 8,
        FunctionCodeWriteMultipleCoils = 15,
        FunctionCodeWriteMultipleRegisters = 16,
    };

    bool is_valid(const std::vector<uint8_t>& data);
    std::vector<uint8_t>& add_checksum(std::vector<uint8_t>& data);

    class Error : public delameta::Error {
    public:
        enum Code {
            InvalidCRC,
            InvalidAddress,
            UnknownRegister,
            UnknownFunctionCode,
            UnknownSubfunction,
            InvalidDataFrame,
            InvalidSetValue,
            ExceptionStatusIsNotDefined,
        };

        using delameta::Error::Error;

        Error(Code code);
        Error(delameta::Error&& err);
        virtual ~Error() = default;
    };

    template <typename T>
    using Result = etl::Result<T, Error>;

    class Client {
    public:
        virtual ~Client() = default;
        virtual Result<std::vector<uint8_t>> request(std::vector<uint8_t> data) = 0;

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
    protected:
        explicit Client(uint8_t server_address) : server_address(server_address) {}
    };

    class Server {
    public:
        virtual ~Server() = default;

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

    protected:
        Server(uint8_t server_address) : server_address(server_address) {}
        Result<std::vector<uint8_t>> execute(const std::vector<uint8_t>& data) const;

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