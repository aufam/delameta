#include <boost/preprocessor.hpp>
#include <fmt/format.h>
#include <delameta/http/server.h>
#include <delameta/modbus/api.h>

static std::vector<uint8_t> hexStringToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;

    for (size_t i = 0; i < hex.length();) {
        if (hex[i] == ' ') {
            ++i;
            continue;
        }

        uint8_t byte = std::stoi(hex.substr(i, 2), nullptr, 16);
        bytes.push_back(byte);
        i += 2;
    }

    return bytes;
}

static std::string bytesToHexString(const std::vector<uint8_t>& bytes) {
    std::string hexString;
    hexString.reserve(bytes.size() * 3);

    for (uint8_t byte : bytes) {
        fmt::format_to(std::back_inserter(hexString), "{:02x} ", byte);
    }

    return hexString;
}

using namespace Project;
namespace http = delameta::http;
namespace modbus = delameta::modbus;
using etl::Ok;
using etl::Err;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/modbus_encode", ("POST")),
    (modbus_encode), (std::string, hex, http::arg::body),
    (http::Server::Result<std::string>)
) {
    try {
        auto vec = hexStringToBytes(hex);
        return Ok(bytesToHexString(modbus::add_checksum(vec)));
    } catch (const std::exception& e) {
        return Err(http::Server::Error{http::StatusBadRequest, e.what()});
    }
}