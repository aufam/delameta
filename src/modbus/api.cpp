#include "delameta/modbus/api.h"
#include "../delameta.h"

using namespace Project;
using namespace Project::delameta;

using etl::Ok;
using etl::Err;

static uint16_t calculate_crc(std::string_view data);
static std::string uint8_to_hex(uint8_t value) {
    const char hexDigits[] = "0123456789ABCDEF";
    char hexString[3];  // 2 characters for hex and 1 for null terminator

    // Convert the high nibble (4 bits) to a hex digit
    hexString[0] = hexDigits[(value >> 4) & 0x0F];

    // Convert the low nibble (4 bits) to a hex digit
    hexString[1] = hexDigits[value & 0x0F];

    // Null terminate the string
    hexString[2] = '\0';

    // Return as a std::string
    return std::string(hexString);
}

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

bool modbus::is_valid(const std::vector<uint8_t>& data) {
    auto size = data.size();
    if (size < 2) return false;

    auto crc = calculate_crc(std::string_view(reinterpret_cast<const char*>(data.data()), size - 2));
    return crc == (data[size - 2] | data[size - 1] << 8);
}

std::vector<uint8_t>& modbus::add_checksum(std::vector<uint8_t>& data) {
    auto crc = calculate_crc(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
    data.push_back((crc >> 0) & 0xff);
    data.push_back((crc >> 8) & 0xff);
    return data;
}

modbus::Error::Error(Code code) : delameta::Error(int(code), "") {
    switch (code) {
        case InvalidCRC: what = "Invalid CRC"; return;
        case InvalidAddress: what = "Invalid address"; return;
        case UnknownRegister: what = "Unknown register"; return;
        case UnknownFunctionCode: what = "Unknown function code"; return;
        case UnknownSubfunction: what = "Unknown sub function"; return;
        case InvalidDataFrame: what = "Invalid data frame"; return;
        case InvalidSetValue: what = "Invalid set value"; return;
        case ExceptionStatusIsNotDefined: what = "Unknown status getter is not defined"; return;
        default: what = "Unknown error code"; return;
    }
}

modbus::Error::Error(delameta::Error&& err) : delameta::Error(std::move(err)) {}

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

auto modbus::Server::execute(const std::vector<uint8_t>& data) const -> Result<std::vector<uint8_t>> {
    if (not modbus::is_valid(data)) 
        return Err(Error::InvalidCRC);

    auto address = data[0];
    auto function_code = data[1];

    if (address != server_address)
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

template <typename T>
static auto read_request_helper(modbus::Client* cli, uint8_t code, uint16_t reg, uint16_t n) -> modbus::Result<std::vector<T>> {
    constexpr bool is_bool = std::is_same_v<T, bool>;
    constexpr bool is_uint16 = std::is_same_v<T, uint16_t>;
    static_assert(is_bool || is_uint16);

    auto req = std::vector<uint8_t>();
    req.reserve(8);
    req.push_back(cli->server_address);
    req.push_back(code);
    req.push_back((reg >> 8) & 0xff);
    req.push_back((reg >> 0) & 0xff);
    req.push_back((n >> 8) & 0xff);
    req.push_back((n >> 0) & 0xff);
    modbus::add_checksum(req);

    return cli->request(std::move(req)).and_then([n](std::vector<uint8_t> res) -> modbus::Result<std::vector<T>> {
        if (res.size() < 6) return Err(modbus::Error::InvalidDataFrame);

        const uint8_t length = res[2];
        if (res.size() != 3 + length + 2) return Err(modbus::Error::InvalidDataFrame);

        if constexpr (is_bool) if ((n + 7) / 8 != length) return Err(modbus::Error::InvalidDataFrame);
        else if (n * 2 != length) return Err(modbus::Error::InvalidDataFrame);
        
        auto ptr = &res[3];
        auto result = std::vector<T>(n);
        if constexpr (is_bool) {
            int bit_index = 0;
            for (int i : etl::range(n)) {
                result[i] = (*ptr) & (1 << bit_index++);
                if (bit_index == 8) (bit_index = 0, ++ptr);
            }
        } else {
            for (int i : etl::range(n)) {
                result[i]  = (*ptr++) << 8;
                result[i] |= (*ptr++);
            }
        }
        return Ok(std::move(result));
    });
}

auto modbus::Client::ReadCoils(uint16_t register_address, uint16_t n_register) -> Result<std::vector<bool>> {
    return read_request_helper<bool>(this, FunctionCodeReadCoils, register_address, n_register);
}

auto modbus::Client::ReadDiscreteInputs(uint16_t register_address, uint16_t n_register) -> Result<std::vector<bool>> {
    return read_request_helper<bool>(this, FunctionCodeReadDiscreteInputs, register_address, n_register);
}

auto modbus::Client::ReadHoldingRegisters(uint16_t register_address, uint16_t n_register) -> Result<std::vector<uint16_t>> {
    return read_request_helper<uint16_t>(this, FunctionCodeReadHoldingRegisters, register_address, n_register);
}

auto modbus::Client::ReadInputRegisters(uint16_t register_address, uint16_t n_register) -> Result<std::vector<uint16_t>> {
    return read_request_helper<uint16_t>(this, FunctionCodeReadInputRegisters, register_address, n_register);
}

static auto write_single_request_helper(modbus::Client* cli, uint8_t code, uint16_t reg, uint16_t value) -> modbus::Result<void> {
    auto req = std::vector<uint8_t>();
    req.reserve(8);
    req.push_back(cli->server_address);
    req.push_back(code);
    req.push_back((reg >> 8) & 0xff);
    req.push_back((reg >> 0) & 0xff);
    req.push_back((value >> 8) & 0xff);
    req.push_back((value >> 0) & 0xff);
    modbus::add_checksum(req);

    return cli->request(std::move(req)).and_then([](std::vector<uint8_t> res) -> modbus::Result<void> {
        if (res.size() != 8) return Err(modbus::Error::InvalidDataFrame);
        return Ok();
    });
}

auto modbus::Client::WriteSingleCoil(uint16_t register_address, bool value) -> Result<void> {
    return write_single_request_helper(this, FunctionCodeWriteSingleCoil, register_address, value ? 0xff00 : 0x0000);
}

auto modbus::Client::WriteSingleRegister(uint16_t register_address, uint16_t value) -> Result<void> {
    return write_single_request_helper(this, FunctionCodeWriteSingleRegister, register_address, value);
}

template<typename T>
static auto write_multiple_request_helper(modbus::Client* cli, uint8_t code, uint16_t reg, const std::vector<T>& values) -> modbus::Result<void> {
    const uint16_t size = values.size();
    const uint8_t length = calculate_bytes_length<T>(size);

    auto req = std::vector<uint8_t>();
    req.reserve(7 + length + 2);
    req.push_back(cli->server_address);
    req.push_back(code);
    req.push_back((reg >> 8) & 0xff);
    req.push_back((reg >> 0) & 0xff);
    req.push_back((size >> 8) & 0xff);
    req.push_back((size >> 0) & 0xff);
    req.push_back(length);
    req.insert(req.end(), length, 0);

    auto ptr = &req[7];
    int bit_count = 0;
    for (auto value : values) write_value_into_buffer(value, ptr, bit_count);
    modbus::add_checksum(req);

    return cli->request(std::move(req)).and_then([](std::vector<uint8_t> res) -> modbus::Result<void> {
        if (res.size() != 8) return Err(modbus::Error::InvalidDataFrame);
        return Ok();
    });
}

auto modbus::Client::WriteMultipleCoils(uint16_t register_address, const std::vector<bool>& values) -> Result<void> {
    return write_multiple_request_helper(this, FunctionCodeWriteMultipleCoils, register_address, values);
}

auto modbus::Client::WriteMultipleRegisters(uint16_t register_address, const std::vector<uint16_t>& values) -> Result<void> {
    return write_multiple_request_helper(this, FunctionCodeWriteMultipleRegisters, register_address, values);
}

auto modbus::Client::ReadExceptionStatus() -> Result<uint8_t> {
    auto req = std::vector<uint8_t>();
    req.reserve(4);
    req.push_back(server_address);
    req.push_back(FunctionCodeReadExceptionStatus);
    add_checksum(req);

    return request(std::move(req)).and_then([](std::vector<uint8_t> res) -> Result<uint8_t> {
        if (res.size() != 5) return Err(Error::InvalidDataFrame);
        if (res[1] != FunctionCodeReadExceptionStatus) return Err(Error::InvalidDataFrame);
        return Ok(res[2]);
    });
}

auto modbus::Client::Diagnostic(uint16_t sub_function, uint16_t input) -> Result<uint16_t> {
    auto req = std::vector<uint8_t>();
    req.reserve(8);
    req.push_back(server_address);
    req.push_back(FunctionCodeDiagnostic);
    req.push_back((sub_function >> 8) & 0xff);
    req.push_back((sub_function >> 0) & 0xff);
    req.push_back((input >> 8) & 0xff);
    req.push_back((input >> 0) & 0xff);
    add_checksum(req);

    return request(std::move(req)).and_then([](std::vector<uint8_t> res) -> modbus::Result<uint16_t> {
        if (res.size() != 8) return Err(Error::InvalidDataFrame);
        if (res[1] != FunctionCodeDiagnostic) return Err(Error::InvalidDataFrame);
        return Ok((res[5] << 8) | res[6]);
    });
}

static const uint16_t crcTable[] = {
        0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
        0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
        0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
        0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
        0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
        0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
        0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
        0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
        0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
        0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
        0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
        0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
        0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
        0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
        0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
        0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
        0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
        0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
        0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
        0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
        0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
        0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
        0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
        0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
        0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
        0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
        0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
        0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
        0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
        0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
        0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
        0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
};

static uint16_t calculate_crc(std::string_view data) {
    uint16_t res = 0xFFFF;
    for (auto byte : data) {
        uint8_t temp = byte ^ res;
        res >>= 8;
        res ^= crcTable[temp];
    }
    return res;
}