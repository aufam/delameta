#include "delameta/modbus/client.h"
#include "../delameta.h"

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

auto modbus::Client::request(std::vector<uint8_t> data) -> Result<std::vector<uint8_t>> {
    if (!is_valid(data)) return Err(Error::InvalidCRC);
    uint8_t addr = data[0];
    uint8_t code = data[1];

    Stream s;
    s << std::move(data);

    return session.request(s).except([](delameta::Error err) {
        return modbus::Error(std::move(err));
    }).and_then([this, addr, code](std::vector<uint8_t> res) -> Result<std::vector<uint8_t>> {
        if (!is_valid(res)) return Err(Error::InvalidCRC);
        if (res[0] != addr) return Err(Error::InvalidAddress);
        if (res[1] == (code | 0x80)) Err(Error::UnknownFunctionCode);
        if (res[1] != code) return Err(Error::UnknownFunctionCode);
        return Ok(std::move(res));
    }).except([this](modbus::Error err) {
        warning(__FILE__, __LINE__, err.what);
        return err;
    });
}


template <typename T>
static auto read_request_helper(modbus::Client* self, uint8_t code, uint16_t reg, uint16_t n) -> modbus::Result<std::vector<T>> {
    constexpr bool is_bool = std::is_same_v<T, bool>;
    constexpr bool is_uint16 = std::is_same_v<T, uint16_t>;
    static_assert(is_bool || is_uint16);

    auto req = std::vector<uint8_t>();
    req.reserve(8);
    req.push_back(self->server_address);
    req.push_back(code);
    req.push_back((reg >> 8) & 0xff);
    req.push_back((reg >> 0) & 0xff);
    req.push_back((n >> 8) & 0xff);
    req.push_back((n >> 0) & 0xff);
    modbus::add_checksum(req);

    return self->request(std::move(req)).and_then([n](std::vector<uint8_t> res) -> modbus::Result<std::vector<T>> {
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

static auto write_single_request_helper(modbus::Client* self, uint8_t code, uint16_t reg, uint16_t value) -> modbus::Result<void> {
    auto req = std::vector<uint8_t>();
    req.reserve(8);
    req.push_back(self->server_address);
    req.push_back(code);
    req.push_back((reg >> 8) & 0xff);
    req.push_back((reg >> 0) & 0xff);
    req.push_back((value >> 8) & 0xff);
    req.push_back((value >> 0) & 0xff);
    modbus::add_checksum(req);

    return self->request(std::move(req)).and_then([](std::vector<uint8_t> res) -> modbus::Result<void> {
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
static auto write_multiple_request_helper(modbus::Client* self, uint8_t code, uint16_t reg, const std::vector<T>& values) -> modbus::Result<void> {
    const uint16_t size = values.size();
    const uint8_t length = calculate_bytes_length<T>(size);

    auto req = std::vector<uint8_t>();
    req.reserve(7 + length + 2);
    req.push_back(self->server_address);
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

    return self->request(std::move(req)).and_then([](std::vector<uint8_t> res) -> modbus::Result<void> {
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