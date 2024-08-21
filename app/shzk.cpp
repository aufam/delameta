#include <boost/preprocessor.hpp>
#include <fmt/ranges.h>
#include <delameta/debug.h>
#include <delameta/http/server.h>
#include <delameta/modbus/client.h>
#include <delameta/serial/client.h>
#include <algorithm>
#include <chrono>
#include <thread>


using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using delameta::modbus::Client;
using Session = delameta::serial::Client;
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
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Server::Result<SHZK>)
) {
    auto session = TRY(
        Session::New(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client cli(address, session);
    SHZK shzk;

    auto buf = TRY(cli.ReadHoldingRegisters(0x1001, 6));
    shzk.frequencyRunning  = buf[0];
    shzk.busVoltage        = buf[1] * .1f;
    shzk.outputVoltage     = buf[2];
    shzk.outputCurrent     = buf[3] * .01f;
    shzk.outputPower       = buf[4];
    shzk.outputTorque      = buf[5];

    std::this_thread::sleep_for(23ms);

    buf = TRY(cli.ReadHoldingRegisters(0x100a, 6));
    shzk.analogInput1 = buf[0];
    shzk.analogInput2 = buf[1];
    shzk.analogInput3 = buf[2];
    shzk.loadSpeed    = buf[5];

    return Ok(shzk);
}

static HTTP_ROUTE(
    ("/shzk", ("POST")),
    (cmd_shzk),
        (int             , address, http::arg::default_val("address", 0x01)              )
        (std::string     , port   , http::arg::default_val("port", std::string("auto"))  )
        (int             , baud   , http::arg::default_val("baud", 9600)                 )
        (int             , tout   , http::arg::default_val("tout", 5)                    )
        (std::string_view, cmd    , http::arg::arg("cmd")                                ),
    (http::Server::Result<void>)
) {
    auto session = TRY(
        Session::New(FL, {.port=port, .baud=baud, .timeout=tout})
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
        return cli.WriteSingleRegister(0x2000, value);
    }
    return Err(http::Server::Error{
        http::StatusBadRequest, fmt::format("Commands not found. Available commands are: {}", cmds)
    });
}