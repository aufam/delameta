#include "delameta/modbus/rtu/client.h"
#include "../../delameta.h"

using namespace Project;
using namespace Project::delameta;

using etl::Ok;
using etl::Err;

modbus::rtu::Client::Client(delameta::serial::Client&& other, uint8_t server_address) 
    : delameta::serial::Client(std::move(other)) 
    , modbus::Client(server_address) {}

auto modbus::rtu::Client::New(const char* file, int line, Args args) -> delameta::Result<Client> {
    return delameta::serial::Client::New(file, line, {args.port, args.baud, args.timeout})
    .then([addr=args.server_address](delameta::serial::Client cli) {
        return modbus::rtu::Client(std::move(cli), addr);
    });
}

auto modbus::rtu::Client::request(std::vector<uint8_t> data) -> Result<std::vector<uint8_t>> {
    if (!is_valid(data)) return Err(Error::InvalidCRC);
    uint8_t addr = data[0];
    uint8_t code = data[1];

    Stream s;
    s << std::move(data);

    return delameta::serial::Client::request(s).except([](delameta::Error err) {
        return modbus::Error(std::move(err));
    }).and_then([this, addr, code](std::vector<uint8_t> res) -> Result<std::vector<uint8_t>> {
        if (!is_valid(res)) return Err(Error::InvalidCRC);
        if (res[0] != addr) return Err(Error::InvalidAddress);
        if (res[1] == (code | 0x80)) Err(Error::UnknownFunctionCode);
        if (res[1] != code) return Err(Error::UnknownFunctionCode);
        return Ok(std::move(res));
    }).except([this](modbus::Error err) {
        warning(this->fd->file, this->fd->line, err.what);
        return err;
    });
}