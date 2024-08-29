#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/udp.h>

using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using etl::Ok;
using etl::Err;
using delameta::UDP;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/test/udp", ("POST")),
    (test_udp),
        (std::string_view, body, http::arg::body                  )
        (std::string     , host, http::arg::arg("host")           )
        (int             , tout, http::arg::default_val("tout", 5)),
    (http::Result<std::string>)
) {
    auto udp = TRY(UDP::Open(FL, {.host=host, .timeout=tout}));
    
    TRY(udp.write(body));

    auto res = TRY(udp.read());

    return Ok(std::string(res.begin(), res.end()));
}

static HTTP_ROUTE(
    ("/test/udp/listen", ("GET")),
    (test_udp_listen),
        (std::string     , host, http::arg::arg("host")           )
        (int             , tout, http::arg::default_val("tout", 5)),
    (http::Result<std::string>)
) {
    delameta::Server<UDP> svr;
    svr.handler = [](delameta::Descriptor&, const std::string& peer, const std::vector<uint8_t>& data) -> delameta::Stream {
        delameta::Stream s;
        s << data;
        DBG(delameta::info, "peer = " + peer);
        return s;
    };

    TRY(svr.start(FL, {host, tout}));
    return Ok("Ok");
}