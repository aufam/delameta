#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/modbus/client.h>
#include <delameta/serial.h>
#include <delameta/utils.h>

using namespace Project;
using namespace delameta;
using delameta::modbus::Client;

auto delameta_modbus_read_coils(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<bool>> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));
    auto n_ = TRY(string_num_into<uint16_t>(n));

    Client cli(addr_, session);
    return cli.ReadCoils(reg_, n_);
}

auto delameta_modbus_read_discrete_inputs(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<bool>> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));
    auto n_ = TRY(string_num_into<uint16_t>(n));

    Client cli(addr_, session);
    return cli.ReadDiscreteInputs(reg_, n_);
}

auto delameta_modbus_read_holding_registers(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<uint16_t>> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));
    auto n_ = TRY(string_num_into<uint16_t>(n));

    Client cli(addr_, session);
    return cli.ReadHoldingRegisters(reg_, n_);
}

auto delameta_modbus_read_input_registers(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view n
) -> Result<std::vector<uint16_t>> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));
    auto n_ = TRY(string_num_into<uint16_t>(n));

    Client cli(addr_, session);
    return cli.ReadInputRegisters(reg_, n_);
}

auto delameta_modbus_write_single_coil(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view value
) -> Result<void> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));
    auto value_ = TRY(string_num_into<int>(value));

    Client cli(addr_, session);
    return cli.WriteSingleCoil(reg_, value_);
}

auto delameta_modbus_write_single_register(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    std::string_view value
) -> Result<void> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));
    auto value_ = TRY(string_num_into<uint16_t>(value));

    Client cli(addr_, session);
    return cli.WriteSingleRegister(reg_, value_);
}

auto delameta_modbus_write_multiple_coils(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    const std::vector<std::string_view>& values
) -> Result<void> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));

    std::vector<bool> values_;
    for (auto &value: values) {
        values_.push_back(TRY(string_num_into<int>(value)));
    }
    
    Client cli(addr_, session);
    return cli.WriteMultipleCoils(reg_, values_);
}

auto delameta_modbus_write_multiple_registers(
    StreamSessionClient& session, 
    std::string_view addr, 
    std::string_view reg, 
    const std::vector<std::string_view>& values
) -> Result<void> {
    auto addr_ = TRY(string_num_into<uint8_t>(addr));
    auto reg_ = TRY(string_num_into<uint16_t>(reg));

    std::vector<uint16_t> values_;
    for (auto &value: values) {
        values_.push_back(TRY(string_num_into<uint16_t>(value)));
    }
    
    Client cli(addr_, session);
    return cli.WriteMultipleRegisters(reg_, values_);
}

template <modbus::FunctionCode code>
static auto mbus_read(std::string_view addr, std::string port, int baud, std::string_view reg, std::string_view n) {
    return Serial::Open(FL, {port, baud}).and_then([addr, reg, n](Serial session) {
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
static auto mbus_write_single(std::string_view addr, std::string port, int baud, std::string_view reg, std::string_view value) {
    return Serial::Open(FL, {port, baud}).and_then([addr, reg, value](Serial session) {
        if constexpr (code == modbus::FunctionCodeWriteSingleCoil) {
            return delameta_modbus_write_single_coil(session, addr, reg, value);
        } else if constexpr (code == modbus::FunctionCodeWriteSingleRegister) {
            return delameta_modbus_write_single_register(session, addr, reg, value);
        }
    });
}

template <modbus::FunctionCode code>
static auto mbus_write_multiple(std::string_view addr, std::string port, int baud, std::string_view reg, std::vector<std::string_view> values) {
    return Serial::Open(FL, {port, baud}).and_then([addr, reg, &values](Serial session) {
        if constexpr (code == modbus::FunctionCodeWriteMultipleCoils) {
            return delameta_modbus_write_multiple_coils(session, addr, reg, values);
        } else if constexpr (code == modbus::FunctionCodeWriteMultipleRegisters) {
            return delameta_modbus_write_multiple_registers(session, addr, reg, values);
        }
    });
}

HTTP_SETUP(modbus_rtu, app) {
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

    app.Get("/modbus_rtu/write_single_coil", http_args_write_single, mbus_write_single<modbus::FunctionCodeWriteSingleCoil>);
    app.Get("/modbus_rtu/write_single_register", http_args_write_single, mbus_write_single<modbus::FunctionCodeWriteSingleRegister>);

    app.Get("/modbus_rtu/write_multiple_coils", http_args_write_multiple, mbus_write_multiple<modbus::FunctionCodeWriteMultipleCoils>);
    app.Get("/modbus_rtu/write_multiple_registers", http_args_write_multiple, mbus_write_multiple<modbus::FunctionCodeWriteMultipleRegisters>);
}
