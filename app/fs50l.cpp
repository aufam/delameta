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
using delameta::Serial;
using delameta::modbus::Client;
using etl::Ok;
using etl::Err;

HTTP_EXTERN_OBJECT(app);

JSON_DECLARE(
    (FS50L)
    ,
    (float, frequencyRunning)
    (float, busVoltage)
    (float, outputVoltage)
    (float, outputCurrent)
    (float, outputPower)
    (float, outputTorque)
    (float, runSpeed)
    (int,   faultInfo)
)

static HTTP_ROUTE(
    ("/fs50l", ("GET")),
    (read_fs50l),
        (int        , address, http::arg::default_val("address", 0x01)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Result<FS50L>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client cli(address, session);
    FS50L fs50l;

    auto buf = TRY(cli.ReadHoldingRegisters(0x3001, 7));
    fs50l.frequencyRunning  = buf[0];
    fs50l.busVoltage        = buf[1] * .1f;
    fs50l.outputVoltage     = buf[2] * .1f;
    fs50l.outputCurrent     = buf[3];
    fs50l.outputPower       = buf[4];
    fs50l.outputTorque      = buf[5];
    fs50l.runSpeed          = buf[6];

    std::this_thread::sleep_for(1ms);

    buf = TRY(cli.ReadHoldingRegisters(0x8000, 1));
    fs50l.faultInfo = buf[0];

    return Ok(fs50l);
}

static HTTP_ROUTE(
    ("/fs50l", ("POST")),
    (cmd_fs50l),
        (int             , address, http::arg::default_val("address", 0x01)              )
        (std::string     , port   , http::arg::default_val("port", std::string("auto"))  )
        (int             , baud   , http::arg::default_val("baud", 9600)                 )
        (int             , tout   , http::arg::default_val("tout", 5)                    )
        (std::string_view, cmd    , http::arg::arg("cmd")                                ),
    (http::Result<void>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client cli(address, session);

    const std::string_view cmds[] = {
        "forward_running",
        "reverse_running",
        "forward_jog",
        "reverse_jog",
        "free_stop",
        "decelerate_stop",
        "fault_resetting",
    };

    uint16_t value = 0x0000;
    auto it = std::find_if(std::begin(cmds), std::end(cmds),[&](std::string_view sv) {
        ++value;
        return cmd == sv;
    });

    if (it != std::end(cmds)) {
        return cli.WriteSingleRegister(0x1000, value);
    }
    return Err(http::Error{
        http::StatusBadRequest, fmt::format("Commands not found. Available commands are: {}", cmds)
    });
}