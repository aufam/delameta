// AJ-SR04M

#include <boost/preprocessor.hpp>
#include <fmt/ranges.h>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/serial.h>

using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using delameta::Serial;
using etl::Ok;
using etl::Err;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/aj-sr04", ("GET")),
    (read_ajsr04),
        (std::string, port   , http::arg::default_val("port", std::string("auto"))     )
        (int        , baud   , http::arg::default_val("baud", 9600)                    )
        (int        , tout   , http::arg::default_val("timeout", 1)                    )
        (std::string, mode   , http::arg::default_val("mode", "computer-printing-mode"))
    ,
    (http::Result<std::string>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    const std::string_view modes[] = {
        "automatic-serial-mode",
        "low-power-serial-mode",
        "computer-printing-mode",
    };

    const std::vector<uint8_t> trigger {0x01};

    if (mode == modes[0] || mode == modes[1]) {
        if (mode == modes[1]) {
            TRY(session.write(trigger));
        }

        auto received_data = TRY(session.read());
        if (received_data[0] != 0xff) {
            return Err(http::Error{
                http::StatusInternalServerError,
                fmt::format("Invalid start byte. Expect 0xff got {:02x}", received_data[0])
            });
        }

        uint8_t h_data = received_data[1];
        uint8_t l_data = received_data[2];
        uint8_t sum = received_data[3];
        uint8_t expected_sum = h_data + l_data;
        if (expected_sum != sum) {
            return Err(http::Error{
                http::StatusInternalServerError,
                fmt::format("Invalid check sum. Expect {:02x}, got {:02x}", expected_sum, sum)
            });
        }

        auto distance = h_data << 8 | l_data;
        return Ok(fmt::format("Gap={}mm", distance));
    }
    else if (mode == modes[2]) {
        TRY(session.write(trigger));
        auto received_data = TRY(session.read());
        return Ok(std::string(received_data.begin(), received_data.end()));
    }

    return Err(http::Error{
        http::StatusInternalServerError,
        fmt::format("Unknown mode: {}. Expect: {}", mode, modes)
    });
}
