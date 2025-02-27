// SHZK inverter

#include <boost/preprocessor.hpp>
#include <fmt/ranges.h>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/modbus/client.h>
#include <delameta/serial.h>
#include <algorithm>
#include <chrono>
#include <thread>


using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using delameta::modbus::Client;
using delameta::Serial;
using etl::Ok;
using etl::Err;

HTTP_EXTERN_OBJECT(app);

JSON_DECLARE(
    (SHZK)
    ,
    (float, frequencyRunning)
    (float, busVoltage)
    (float, outputVoltage)
    (float, outputCurrent)
    (float, outputPower)
    (float, outputTorque)
    (float, analogInput1)
    (float, analogInput2)
    (float, analogInput3)
    (float, loadSpeed)
    (int,   state)
    (int,   faultInfo)
)

static HTTP_ROUTE(
    ("/shzk", ("GET")),
    (read_shzk),
        (int        , address, http::arg::default_val("address", 0x01)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("timeout", 1)                 )
    ,
    (http::Result<SHZK>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client c(address, session);
    c.response_length_size_is_16bits = true;
    SHZK shzk {};

    auto buf = TRY(c.ReadHoldingRegisters(0x1001, 6));
    shzk.frequencyRunning  = buf[0];
    shzk.busVoltage        = buf[1] * .1f;
    shzk.outputVoltage     = buf[2];
    shzk.outputCurrent     = buf[3] * .01f;
    shzk.outputPower       = buf[4];
    shzk.outputTorque      = buf[5];

    std::this_thread::sleep_for(23ms);

    buf = TRY(c.ReadHoldingRegisters(0x100a, 6));
    shzk.analogInput1 = buf[0];
    shzk.analogInput2 = buf[1];
    shzk.analogInput3 = buf[2];
    shzk.loadSpeed    = buf[5];

    return Ok(shzk);
}

static HTTP_ROUTE(
    ("/shzk", ("POST")),
    (cmd_shzk),
        (int             , address, http::arg::json_item_default_val("address", 0x01)            )
        (std::string     , port   , http::arg::json_item_default_val("port", std::string("auto")))
        (int             , baud   , http::arg::json_item_default_val("baud", 9600)               )
        (int             , tout   , http::arg::json_item_default_val("timeout", 1)               )
        (std::string_view, cmd    , http::arg::json_item("cmd")                                  ),
    (http::Result<void>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client c(address, session);
    c.response_length_size_is_16bits = true;

    const std::string_view cmds[] = {
        "forwardRunning",
        "reverseRunning",
        "forwardJog",
        "reverseJog",
        "freeStop",
        "decelerateStop",
        "faultResetting",
    };

    uint16_t value = 0x0000;
    auto it = std::find_if(std::begin(cmds), std::end(cmds),[&](std::string_view sv) {
        ++value;
        return cmd == sv;
    });

    if (it != std::end(cmds)) {
        return c.WriteSingleRegister(0x2000, value);
    }

    return Err(http::Error{
        http::StatusBadRequest, fmt::format("Commands not found. Available commands are: {}", cmds)
    });
}
