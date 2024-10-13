// https://www.dypcn.com/uploads/A01-Output-Interfaces.pdf

#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/modbus/client.h>
#include <delameta/serial.h>
#include <thread>
#include <chrono>

using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using delameta::modbus::Client;
using delameta::Serial;
using etl::Ok;
using etl::Err;

HTTP_EXTERN_OBJECT(app);

JSON_DECLARE(
    (A01)
    ,
    (float, distance)
    (float, temperature)
)

static HTTP_ROUTE(
    ("/a01", ("GET")),
    (read_a01),
        (int        , address, http::arg::default_val("address", 0x01)              )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("timeout", 1)                 ),
    (http::Result<A01>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client c(address, session);

    auto res1 = TRY(c.ReadHoldingRegisters(0x0101, 1));
    std::this_thread::sleep_for(10ms);
    auto res2 = TRY(c.ReadHoldingRegisters(0x0102, 1));

    return Ok(A01{
        .distance = res1[0] * 0.1f,
        .temperature = res2[0] * 0.1f,
    });
}

static HTTP_ROUTE(
    ("/a01/set-address", ("POST")),
    (a01_setup),
        (int        , old_address, http::arg::json_item("oldAddress")                           )
        (int        , new_address, http::arg::json_item("newAddress")                           )
        (std::string, port       , http::arg::json_item_default_val("port", std::string("auto")))
        (int        , baud       , http::arg::json_item_default_val("baud", 9600)               )
        (int        , tout       , http::arg::json_item_default_val("timeout", 1)               ),
    (http::Result<std::string>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Client c(old_address, session);

    TRY(c.WriteSingleRegister(0x0200, new_address));  // set new address

    return Ok("Device is setup. Please restart the device\n");
}
