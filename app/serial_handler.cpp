#include <boost/preprocessor.hpp>
#include "delameta/http/server.h"
#include "delameta/serial/client.h"
#include "delameta/debug.h"

using namespace Project;
using namespace delameta::http;
using delameta::serial::Client;
using delameta::Stream;
using delameta::Error;
using etl::Ref;

HTTP_EXTERN_OBJECT(app);

HTTP_ROUTE(
    ("/serial", ("GET")), 
    (serial_handler),
        (std::string        , port   , arg::default_val("port", std::string("auto")))
        (int                , baud   , arg::default_val("baud", 9600)               )
        (int                , timeout, arg::default_val("timeout", 5)               )
        (std::string_view   , data   , arg::arg("data")                             )
        (Ref<ResponseWriter>, res    , arg::response                                ),
    (Server::Result<void>)
) {
    return Client::New(FL, {port, baud, timeout}).and_then([&](Client cli) {
        Stream s;
        s << data;
        return cli.request(s);
    }).then([&](std::vector<uint8_t> data) {
        res->headers["Content-Type"] = "application/octet-stream";
        res->headers["Content-Length"] = std::to_string(data.size());
        res->body_stream << std::move(data);
    });
}
