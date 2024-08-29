#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/serial.h>

using namespace Project;
namespace http = delameta::http;
using delameta::Serial;
using delameta::Stream;
using etl::Ok;
using etl::Ref;

HTTP_EXTERN_OBJECT(app);

HTTP_ROUTE(
    ("/serial", ("POST")), 
    (serial_handler),
        (std::string              , port   , http::arg::default_val("port", std::string("auto")))
        (int                      , baud   , http::arg::default_val("baud", 9600)               )
        (int                      , timeout, http::arg::default_val("timeout", 5)               )
        (std::string_view         , data   , http::arg::body                                    )
        (Ref<http::ResponseWriter>, res    , http::arg::response                                ),
    (http::Result<void>)
) {
    auto cli = TRY(Serial::Open(FL, {port, baud, timeout}));

    Stream s;
    s << data;
    auto response = TRY(cli.request(s));

    res->headers["Content-Type"] = "application/octet-stream";
    res->headers["Content-Length"] = std::to_string(response.size());
    res->body_stream << std::move(response);

    return Ok();
}
