#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/serial.h>

using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using delameta::Serial;
using delameta::Stream;
using etl::Ok;
using etl::Err;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/aj-sr04", ("GET")),
    (read_ajsr04),
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Result<std::string>)
) {
    auto session = TRY(
        Serial::Open(FL, {.port=port, .baud=baud, .timeout=tout})
    );

    Stream s;
    s << std::vector<uint8_t>({0x01});
    auto buf = TRY(session.request(s));

    return Ok(std::string(buf.begin(), buf.end()));
}
