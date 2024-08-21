#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/server.h>
#include <delameta/serial/client.h>

using namespace Project;
using namespace delameta::http;
using delameta::serial::Client;
using delameta::Stream;
using etl::Ok;
using etl::Ref;

HTTP_EXTERN_OBJECT(app);

HTTP_ROUTE(
    ("/serial", ("POST")), 
    (serial_handler),
        (std::string        , port   , arg::default_val("port", std::string("auto")))
        (int                , baud   , arg::default_val("baud", 9600)               )
        (int                , timeout, arg::default_val("timeout", 5)               )
        (std::string_view   , data   , arg::body                                    )
        (Ref<ResponseWriter>, res    , arg::response                                ),
    (Server::Result<void>)
) {
    auto cli = TRY(Client::New(FL, {port, baud, timeout}));

    Stream s;
    s << data;
    auto response = TRY(cli.request(s));

    res->headers["Content-Type"] = "application/octet-stream";
    res->headers["Content-Length"] = std::to_string(response.size());
    res->body_stream << std::move(response);

    return Ok();
}
