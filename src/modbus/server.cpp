#include "delameta/modbus/server.h"
#include "delameta/debug.h"

using namespace Project;
using namespace Project::delameta;

using etl::Ok;
using etl::Err;

template <typename T>
static uint8_t calculate_bytes_length(uint16_t n_register) {
    if constexpr (std::is_same_v<T, bool>) return (n_register + 7) / 8;
    else if constexpr (std::is_same_v<T, uint16_t>) return n_register * 2;
} 

template <typename T>
static void write_value_into_buffer(T value, uint8_t*& ptr, int& bit_count) {
    if constexpr (std::is_same_v<T, bool>) {
        *ptr |= value << bit_count++;
        if (bit_count == 8) (bit_count = 0, ++ptr);
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        *ptr++ = (value >> 8) & 0xff;
        *ptr++ = (value >> 0) & 0xff;
    }
} 

void modbus::Server::bind(StreamSessionServer& server, bool accept_all_address) const {
    server.handler = [this, accept_all_address](Descriptor&, const std::string& name, const std::vector<uint8_t>& data) -> Stream {
        auto res = execute(data, accept_all_address);
        Stream s;
        if (res.is_ok()) {
            if (logger) logger(name, data, res.unwrap());
            s << std::move(res.unwrap());
        } else {
            warning(__FILE__, __LINE__, res.unwrap_err().what);
        }
        return s;
    };
}

auto modbus::Server::CoilGetter(uint16_t register_address, std::function<bool()> getter) -> const std::function<bool()>& {
    return coil_getters[register_address] = std::move(getter);
}

auto modbus::Server::CoilSetter(uint16_t register_address, std::function<void(bool)> setter) -> const std::function<void(bool)>& {
    return coil_setters[register_address] = std::move(setter);
}

auto modbus::Server::HoldingRegisterGetter(uint16_t register_address, std::function<uint16_t()> getter) -> const std::function<uint16_t()>& {
    return holding_register_getters[register_address] = std::move(getter);
}

auto modbus::Server::HoldingRegisterSetter(uint16_t register_address, std::function<void(uint16_t)> setter) -> const std::function<void(uint16_t)>& {
    return holding_register_setters[register_address] = std::move(setter);
}

auto modbus::Server::DiscreteInputGetter(uint16_t register_address, std::function<bool()> getter) -> const std::function<bool()>&  {
    return discrete_input_getters[register_address] = std::move(getter);
}

auto modbus::Server::AnalogInputGetter(uint16_t register_address, std::function<uint16_t()> getter)-> const std::function<uint16_t()>& {
    return analog_input_getters[register_address] = std::move(getter);
}

auto modbus::Server::ExceptionStatusGetter(std::function<uint8_t()> getter) -> const std::function<uint8_t()>& {
    return exception_status_getter = std::move(getter);
}

auto modbus::Server::DiagnosticGetter(uint8_t sub_function, std::function<Result<uint16_t>(uint16_t)> getter) -> const std::function<Result<uint16_t>(uint16_t)>& {
    return diagnostic_getters[sub_function] = std::move(getter);
}

auto modbus::Server::execute(const std::vector<uint8_t>& data, bool accept_all_address) const -> Result<std::vector<uint8_t>> {
    if (not modbus::is_valid(data)) 
        return Err(Error::InvalidCRC);

    auto address = data[0];
    auto function_code = data[1];

    if (!accept_all_address && address != server_address)
        return Err(Error::InvalidAddress);

    std::vector<uint8_t> res;
    return [&]() -> Result<std::vector<uint8_t>> {
        switch (function_code) {
            case FunctionCodeReadCoils: 
                return execute_read_coils(data); 
            case FunctionCodeReadDiscreteInputs: 
                return execute_read_discrete_inputs(data); 
            case FunctionCodeReadHoldingRegisters: 
                return execute_read_holding_registers(data); 
            case FunctionCodeReadInputRegisters: 
                return execute_read_input_registers(data); 
            case FunctionCodeWriteSingleCoil: 
                return execute_write_single_coil(data); 
            case FunctionCodeWriteSingleRegister: 
                return execute_write_single_register(data); 
            case FunctionCodeReadExceptionStatus: 
                return execute_read_exception_status(data); 
            case FunctionCodeDiagnostic: 
                return execute_diagnostic(data); 
            case FunctionCodeWriteMultipleCoils: 
                return execute_write_multiple_coils(data); 
            case FunctionCodeWriteMultipleRegisters: 
                return execute_write_multiple_registers(data);
            default: 
                return Err(Error::UnknownFunctionCode);
        }
    }().then([](std::vector<uint8_t> res) { return add_checksum(res); });
}

template <typename T>
static auto read_helper(
    const std::vector<uint8_t>& data, 
    const std::unordered_map<uint16_t, std::function<T()>>& getter
) -> modbus::Result<std::vector<uint8_t>> {
    if (data.size() != 8) return Err(modbus::Error::InvalidDataFrame);
    const uint16_t requested_length = data[4] << 8 | data[5];
    const size_t getter_size = getter.size();
    if (requested_length == 0) return Err(modbus::Error::InvalidDataFrame);
    if (getter_size == 0 or requested_length > getter_size) return Err(modbus::Error::UnknownRegister);

    const uint16_t start_register = data[2] << 8 | data[3];
    uint8_t length = calculate_bytes_length<T>(requested_length);

    auto res = std::vector<uint8_t>();
    res.reserve(3 + length + 2);
    res.push_back(data[0]);
    res.push_back(data[1]);
    res.push_back(length);
    res.insert(res.end(), length, 0);

    auto ptr = &res[3];
    int bit_count = 0;
    for (uint16_t reg : etl::range<uint16_t>(start_register, start_register + requested_length)) {
        auto it = getter.find(reg);
        if (it == getter.end()) return Err(modbus::Error::UnknownRegister);
        write_value_into_buffer(it->second(), ptr, bit_count);
    }

    return Ok(std::move(res));
}

auto modbus::Server::execute_read_coils(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return read_helper(data, coil_getters);
}

auto modbus::Server::execute_read_discrete_inputs(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return read_helper(data, discrete_input_getters);
}

auto modbus::Server::execute_read_holding_registers(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return read_helper(data, holding_register_getters);
}

auto modbus::Server::execute_read_input_registers(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return read_helper(data, analog_input_getters);
}

template <typename T>
static auto write_single_helper(
    const std::vector<uint8_t>& data, 
    const std::unordered_map<uint16_t, std::function<void(T)>>& setter
) -> modbus::Result<std::vector<uint8_t>> {
    constexpr bool is_bool = std::is_same_v<T, bool>;
    constexpr bool is_uint16 = std::is_same_v<T, uint16_t>;
    static_assert(is_bool || is_uint16);

    if (data.size() != 8) return Err(modbus::Error::InvalidDataFrame);
    const uint16_t start_register = data[2] << 8 | data[3];
    const uint16_t value = data[4] << 8 | data[5];

    auto it = setter.find(start_register);
    if (it == setter.end()) return Err(modbus::Error::UnknownRegister);

    if constexpr (is_bool) {
        if (value == 0xFF00) it->second(true);
        else if (value == 0x0000) it->second(false);
        else return Err(modbus::Error::InvalidDataFrame);
    } else {
        it->second(value);
    }

    auto res = std::vector<uint8_t>();
    res.reserve(6 + 2);
    res.insert(res.end(), data.begin(), data.begin() + 6);

    return Ok(std::move(res));
}

auto modbus::Server::execute_write_single_coil(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return write_single_helper(data, coil_setters);
}

auto modbus::Server::execute_write_single_register(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return write_single_helper(data, holding_register_setters);
}

template <typename T>
static auto write_multiple_helper(
    const std::vector<uint8_t>& data, 
    const std::unordered_map<uint16_t, std::function<void(T)>>& setter
) -> modbus::Result<std::vector<uint8_t>> {
    constexpr bool is_bool = std::is_same_v<T, bool>;
    constexpr bool is_uint16 = std::is_same_v<T, uint16_t>;
    static_assert(is_bool || is_uint16);

    if (data.size() <= 9) return Err(modbus::Error::InvalidDataFrame);
    const uint16_t requested_length = data[4] << 8 | data[5];
    const size_t setter_size = setter.size();
    if (requested_length == 0) return Err(modbus::Error::InvalidDataFrame);
    if (setter_size == 0 or requested_length > setter_size) return Err(modbus::Error::UnknownRegister);

    const uint16_t start_register = data[2] << 8 | data[3];
    const uint8_t n_bytes = data[6];
    const uint8_t length = calculate_bytes_length<T>(requested_length);
    if (length != n_bytes) return Err(modbus::Error::InvalidDataFrame);

    int bit_index = 0;
    int buf_index = 7;
    for (uint16_t reg : etl::range<uint16_t>(start_register, start_register + requested_length)) {
        auto it = setter.find(reg);
        if (it == setter.end()) return Err(modbus::Error::UnknownRegister);

        if constexpr (is_bool) {
            it->second((1 << bit_index++) & data[buf_index]);
            if (bit_index == 8) (bit_index = 0, ++buf_index);
        } else {
            it->second(data[buf_index] << 8 | data[buf_index + 1]);
            buf_index += 2;
        }
    }

    auto res = std::vector<uint8_t>();
    res.reserve(6 + 2);
    res.insert(res.end(), data.begin(), data.begin() + 6);

    return Ok(std::move(res));
}

auto modbus::Server::execute_write_multiple_coils(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return write_multiple_helper(data, coil_setters);
}

auto modbus::Server::execute_write_multiple_registers(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    return write_multiple_helper(data, holding_register_setters);
}

auto modbus::Server::execute_read_exception_status(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    if (data.size() != 4) return Err(Error::InvalidDataFrame);
    if (not exception_status_getter) return Err(Error::ExceptionStatusIsNotDefined);

    auto res = std::vector<uint8_t>();
    res.reserve(3 + 2);
    res.push_back(data[0]);
    res.push_back(data[1]);
    res.push_back(exception_status_getter());
    return Ok(std::move(res));
}

auto modbus::Server::execute_diagnostic(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    if (data.size() != 8) return Err(Error::InvalidDataFrame);
    uint16_t sub_function = data[2] << 8 | data[3];
    uint16_t input = data[4] << 8 | data[5];

    auto it = diagnostic_getters.find(sub_function);
    if (it == diagnostic_getters.end())
        return Err(Error::UnknownSubfunction);
    
    return it->second(input).then([&data](uint16_t output) {
        auto res = std::vector<uint8_t>();
        res.reserve(8);
        res.push_back(data[0]);
        res.push_back(data[1]);
        res.push_back(data[2]);
        res.push_back(data[3]);
        res.push_back((output >> 8) & 0xff);
        res.push_back((output >> 0) & 0xff);
        return res;
    });
}