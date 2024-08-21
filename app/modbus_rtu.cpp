#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/server.h>
#include <delameta/modbus/client.h>
#include <delameta/serial/client.h>

using namespace Project;
using namespace delameta;
using delameta::modbus::Client;
using Session = delameta::serial::Client;

template <modbus::FunctionCode code>
static auto mbus_read(int address, std::string port, int baud, uint16_t reg, uint16_t n) {
    return Session::New(__FILE__, __LINE__, {port, baud}).and_then([address, reg, n](Session session) {
        Client cli(address, session);
        if constexpr (code == modbus::FunctionCodeReadCoils) return cli.ReadCoils(reg, n);
        else if constexpr (code == modbus::FunctionCodeReadDiscreteInputs) return cli.ReadDiscreteInputs(reg, n);
        else if constexpr (code == modbus::FunctionCodeReadHoldingRegisters) return cli.ReadHoldingRegisters(reg, n);
        else if constexpr (code == modbus::FunctionCodeReadInputRegisters) return cli.ReadInputRegisters(reg, n);
    });
}

template <modbus::FunctionCode code, typename T>
static auto mbus_write_single(int address, std::string port, int baud, uint16_t reg, T value) {
    return Session::New(__FILE__, __LINE__, {port, baud}).and_then([address, reg, value](Session session) {
        Client cli(address, session);
        if constexpr (code == modbus::FunctionCodeWriteSingleCoil) return cli.WriteSingleCoil(reg, value);
        else if constexpr (code == modbus::FunctionCodeWriteSingleRegister) return cli.WriteSingleRegister(reg, value);
    });
}

template <modbus::FunctionCode code, typename T>
static auto mbus_write_multiple(int address, std::string port, int baud, uint16_t reg, std::vector<T> values) {
    return Session::New(__FILE__, __LINE__, {port, baud}).and_then([address, reg, &values](Session session) {
        Client cli(address, session);
        if constexpr (code == modbus::FunctionCodeWriteMultipleCoils) return cli.WriteMultipleCoils(reg, values);
        else if constexpr (code == modbus::FunctionCodeWriteMultipleRegisters) return cli.WriteMultipleRegisters(reg, values);
    });
}

HTTP_SETUP(app) {
    static const auto http_args = std::tuple{
        http::arg::arg("address"),
        http::arg::default_val("port", std::string("auto")), 
        http::arg::default_val("baud", 9600),
        http::arg::arg("reg"),
        http::arg::arg("n"),
    };

    static const auto http_args_write_single = std::tuple{
        http::arg::arg("address"),
        http::arg::default_val("port", std::string("auto")), 
        http::arg::default_val("baud", 9600),
        http::arg::arg("reg"),
        http::arg::arg("value"),
    };

    static const auto http_args_write_multiple = std::tuple{
        http::arg::arg("address"),
        http::arg::default_val("port", std::string("auto")), 
        http::arg::default_val("baud", 9600),
        http::arg::arg("reg"),
        http::arg::arg("values"),
    };

    app.Get("/modbus_rtu/read_coils", http_args, mbus_read<modbus::FunctionCodeReadCoils>);
    app.Get("/modbus_rtu/read_discrete_inputs", http_args, mbus_read<modbus::FunctionCodeReadDiscreteInputs>);
    app.Get("/modbus_rtu/read_holding_registers", http_args, mbus_read<modbus::FunctionCodeReadHoldingRegisters>);
    app.Get("/modbus_rtu/read_input_registers", http_args, mbus_read<modbus::FunctionCodeReadInputRegisters>);

    app.Get("/modbus_rtu/write_single_coil", http_args_write_single, mbus_write_single<modbus::FunctionCodeWriteSingleCoil, bool>);
    app.Get("/modbus_rtu/write_single_register", http_args_write_single, mbus_write_single<modbus::FunctionCodeWriteSingleRegister, uint16_t>);

    app.Get("/modbus_rtu/write_multiple_coils", http_args_write_multiple, mbus_write_multiple<modbus::FunctionCodeWriteMultipleCoils, bool>);
    app.Get("/modbus_rtu/write_multiple_registers", http_args_write_multiple, mbus_write_multiple<modbus::FunctionCodeWriteMultipleRegisters, uint16_t>);
}