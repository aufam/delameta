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
        (std::string, port   , http::arg::default_val("port", std::string("auto")))
        (int        , baud   , http::arg::default_val("baud", 9600)               )
        (int        , timeout, http::arg::default_val("timeout", 5)               )
        (Stream     , data   , http::arg::body                                    ),
    (http::Result<std::vector<uint8_t>>)
) {
    auto cli = TRY(Serial::Open(FL, {port, baud, timeout}));
    return cli.request(data);
}
