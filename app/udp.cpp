#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/udp.h>
#include <thread>

using namespace Project;
using namespace std::literals;
namespace http = delameta::http;
using etl::Ok;
using etl::Err;
using delameta::Stream;
using delameta::UDP;
using delameta::Server;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/udp", ("POST")),
    (test_udp),
        (Stream     , body, http::arg::body                     )
        (std::string, host, http::arg::arg("host")              )
        (int        , tout, http::arg::default_val("timeout", 5)),
    (http::Result<std::vector<uint8_t>>)
) {
    auto udp = TRY(UDP::Open(FL, {.host=host, .as_server=false, .timeout=tout}));
    return udp.request(body);
}

static std::function<void()> server_stopper;

static HTTP_ROUTE(
    ("/test/udp/listen", ("GET")),
    (test_udp_listen),
        (std::string, host, http::arg::arg("host")              )
        (int        , tout, http::arg::default_val("timeout", 5)),
    (http::Result<void>)
) {
    Server<UDP> svr;
    svr.handler = [](delameta::Descriptor&, const std::string& peer, const std::vector<uint8_t>& data) -> Stream {
        Stream s;
        s << "UDP response: ";
        s << data;
        INFO("peer = " + peer);
        return s;
    };

    std::thread([=, svr=std::move(svr)]() mutable {
        server_stopper = [&]() {
            return svr.stop();
        };

        svr.start(FL, {host, tout});
        server_stopper = {};
    }).detach();

    return Ok();
}

static HTTP_ROUTE(
    ("/test/udp/stop", ("GET")),
    (test_udp_stop),,
    (void)
) {
    if (server_stopper) server_stopper();
}
