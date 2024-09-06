#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/file.h>

using namespace Project;
namespace http = delameta::http;
using delameta::File;
using etl::Ok;
using etl::Err;
using etl::Ref;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/", ("GET")),
    (home), (Ref<http::ResponseWriter>, res, http::arg::response),
    (http::Result<void>)
) {
    auto file = TRY(
        File::Open(FL, {DELAMETA_HOME_DIRECTORY "/app/README.html"}).or_except([](auto) {
            return File::Open(FL, {"/usr/share/delameta/assets/index.html"});
        })
    );

    res->headers["Content-Length"] = std::to_string(file.file_size());
    res->headers["Content-Type"] = "text/html";
    file >> res->body_stream;

    return Ok();
}

static HTTP_ROUTE(
    ("/readme", ("GET")),
    (readme), (Ref<http::ResponseWriter>, res, http::arg::response),
    (http::Result<void>)
) {
    auto file = TRY(
        File::Open(FL, {DELAMETA_HOME_DIRECTORY "/app/README.md"}).or_except([](auto) {
            return File::Open(FL, {"/usr/share/delameta/assets/README.md"});
        })
    );

    res->headers["Content-Length"] = std::to_string(file.file_size());
    res->headers["Content-Type"] = "text/markdown";
    file >> res->body_stream;

    return Ok();
}