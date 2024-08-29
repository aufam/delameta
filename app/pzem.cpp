#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/modbus/client.h>
#include <delameta/serial.h>
#include <chrono>
#include <thread>

using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using delameta::Serial;
using delameta::Stream;
using delameta::modbus::Client;
using etl::Ok;
using etl::Err;

HTTP_EXTERN_OBJECT(app);

JSON_DECLARE(
    (PZEM)
    ,
    (float, voltage)
    (float, current)
    (float, power)
    (float, energy)
    (float, frequency)
    (float, powerFactor)
    (bool , alarm)
    (float, alarmThreshold)
)

static HTTP_ROUTE(
    ("/pzem", ("GET")),
    (read_pzem),
        (int        , address, http::arg::default_val("address", 0xf8)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Result<PZEM>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client cli(address, session);
    PZEM pzem;

    auto buf = TRY(cli.ReadInputRegisters(0x0000, 10));
    pzem.voltage     = buf[0] * .1f;
    pzem.current     = (buf[1] | buf[2] << 16) * .001f;
    pzem.power       = (buf[3] | buf[4] << 16) * .1f;
    pzem.energy      = (buf[5] | buf[6] << 16) * 1.f;
    pzem.energy      = (buf[5] | buf[6] << 16) * 1.f;
    pzem.frequency   = buf[7] * .1f;
    pzem.powerFactor = buf[8] * .01f;
    pzem.alarm       = buf[9] == 0xffff;

    buf = TRY(cli.ReadHoldingRegisters(0x0001, 1));
    pzem.alarmThreshold = buf[0];

    return Ok(pzem);
}

static HTTP_ROUTE(
    ("/pzem/calibrate", ("GET")),
    (pzem_calibrate),
        (int        , address, http::arg::default_val("address", 0xf8)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Result<void>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    std::vector<uint8_t> buf { uint8_t(address), 0x41, 0x37, 0x21 };
    delameta::modbus::add_checksum(buf);

    Stream s;
    s << std::move(buf);
    TRY(session.request(s));

    return Ok();
}

static HTTP_ROUTE(
    ("/pzem/reset_energy", ("GET")),
    (pzem_reset_energy),
        (int        , address, http::arg::default_val("address", 0xf8)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Result<void>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    std::vector<uint8_t> buf { uint8_t(address), 0x42 };
    delameta::modbus::add_checksum(buf);

    Stream s;
    s << std::move(buf);
    TRY(session.request(s));

    return Ok();
}