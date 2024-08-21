#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/server.h>
#include <delameta/modbus/client.h>
#include <delameta/serial/client.h>
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
    (URM15)
    ,
    (float, distance)
    (float, temperature)
)

static HTTP_ROUTE(
    ("/urm15", ("GET")),
    (read_urm15),
        (int        , address, http::arg::default_val("address", 0x0f)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Server::Result<URM15>)
) {
    auto session = TRY(
        Session::New(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client cli(address, session);
    
    cli.WriteSingleRegister(0x0008, 0b1101); // send trigger
    std::this_thread::sleep_for(65ms);

    auto res = TRY(cli.ReadHoldingRegisters(0x0005, 2));
    return Ok(URM15{
        .distance = res[0] * 0.1f,
        .temperature = res[1] * 0.1f,
    });
}

static HTTP_ROUTE(
    ("/urm15/setup", ("GET")),
    (urm15_setup),
        (int        , address, http::arg::default_val("address", 0x0f)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 19200)                )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Server::Result<std::string>)
) {
    Session session = TRY(
        Session::New(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client cli(0x00, session);

    TRY(cli.WriteSingleRegister(0x0008, 0b0101));  // set control register
    TRY(cli.WriteSingleRegister(0x0002, address)); // set device address
    TRY(cli.WriteSingleRegister(0x0003, 3)); // set baudrate to 9600

    return Ok("Device is setup. Please restart the device\n");
}