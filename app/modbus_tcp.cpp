#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/modbus/client.h>
#include <delameta/tcp.h>

using namespace Project;
using namespace delameta;
using delameta::modbus::Client;


auto delameta_modbus_read_coils(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<bool>>;

auto delameta_modbus_read_discrete_inputs(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<bool>>;

auto delameta_modbus_read_holding_registers(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<uint16_t>>;

auto delameta_modbus_read_input_registers(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<uint16_t>>;

auto delameta_modbus_write_single_coil(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view value
) -> Result<void>;

auto delameta_modbus_write_single_register(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view value
) -> Result<void>;

auto delameta_modbus_write_multiple_coils(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    const std::vector<std::string_view>& values
) -> Result<void>;

auto delameta_modbus_write_multiple_registers(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    const std::vector<std::string_view>& values
) -> Result<void>;

template <modbus::FunctionCode code>
static auto mbus_read(std::string_view addr, std::string host, int timeout, std::string_view reg, std::string_view n) {
    return TCP::Open(FL, {host, timeout}).and_then([addr, reg, n](TCP session) {
        if constexpr (code == modbus::FunctionCodeReadCoils) {
            return delameta_modbus_read_coils(session, addr, reg, n);
        } else if constexpr (code == modbus::FunctionCodeReadDiscreteInputs) {
            return delameta_modbus_read_discrete_inputs(session, addr, reg, n);
        } else if constexpr (code == modbus::FunctionCodeReadHoldingRegisters) {
            return delameta_modbus_read_holding_registers(session, addr, reg, n);
        } else if constexpr (code == modbus::FunctionCodeReadInputRegisters) {
            return delameta_modbus_read_input_registers(session, addr, reg, n);
        }
    });
}

template <modbus::FunctionCode code>
static auto mbus_write_single(std::string_view addr, std::string host, int timeout, std::string_view reg, std::string_view value) {
    return TCP::Open(FL, {host, timeout}).and_then([addr, reg, value](TCP session) {
        if constexpr (code == modbus::FunctionCodeWriteSingleCoil) {
            return delameta_modbus_write_single_coil(session, addr, reg, value);
        } else if constexpr (code == modbus::FunctionCodeWriteSingleRegister) {
            return delameta_modbus_write_single_register(session, addr, reg, value);
        }
    });
}

template <modbus::FunctionCode code>
static auto mbus_write_multiple(std::string_view addr, std::string host, int timeout, std::string_view reg, std::vector<std::string_view> values) {
    return TCP::Open(FL, {host, timeout}).and_then([addr, reg, &values](TCP session) {
        if constexpr (code == modbus::FunctionCodeWriteMultipleCoils) {
            return delameta_modbus_write_multiple_coils(session, addr, reg, values);
        } else if constexpr (code == modbus::FunctionCodeWriteMultipleRegisters) {
            return delameta_modbus_write_multiple_registers(session, addr, reg, values);
        }
    });
}

HTTP_SETUP(modbus_tcp, app) {
    static const auto http_args = std::tuple{
        http::arg::default_val("address", "0xff"),
        http::arg::arg("host"), 
        http::arg::default_val("timeout", 5),
        http::arg::arg("reg"),
        http::arg::arg("n"),
    };

    static const auto http_args_write_single = std::tuple{
        http::arg::default_val("address", "0xff"),
        http::arg::arg("host"), 
        http::arg::default_val("timeout", 5),
        http::arg::arg("reg"),
        http::arg::arg("value"),
    };

    static const auto http_args_write_multiple = std::tuple{
        http::arg::default_val("address", "0xff"),
        http::arg::arg("host"), 
        http::arg::default_val("timeout", 5),
        http::arg::arg("reg"),
        http::arg::arg("values"),
    };

    app.Get("/modbus_tcp/read_coils", http_args, mbus_read<modbus::FunctionCodeReadCoils>);
    app.Get("/modbus_tcp/read_discrete_inputs", http_args, mbus_read<modbus::FunctionCodeReadDiscreteInputs>);
    app.Get("/modbus_tcp/read_holding_registers", http_args, mbus_read<modbus::FunctionCodeReadHoldingRegisters>);
    app.Get("/modbus_tcp/read_input_registers", http_args, mbus_read<modbus::FunctionCodeReadInputRegisters>);

    app.Get("/modbus_tcp/write_single_coil", http_args_write_single, mbus_write_single<modbus::FunctionCodeWriteSingleCoil>);
    app.Get("/modbus_tcp/write_single_register", http_args_write_single, mbus_write_single<modbus::FunctionCodeWriteSingleRegister>);

    app.Get("/modbus_tcp/write_multiple_coils", http_args_write_multiple, mbus_write_multiple<modbus::FunctionCodeWriteMultipleCoils>);
    app.Get("/modbus_tcp/write_multiple_registers", http_args_write_multiple, mbus_write_multiple<modbus::FunctionCodeWriteMultipleRegisters>);
}
