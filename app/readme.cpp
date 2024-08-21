#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/server.h>
#include <delameta/file_descriptor.h>

using namespace Project;
namespace http = delameta::http;
using delameta::FileDescriptor;
using etl::Ok;
using etl::Err;
using etl::Ref;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/", ("GET")),
    (home), (Ref<http::ResponseWriter>, res, http::arg::response),
    (http::Server::Result<void>)
) {
    auto file = TRY(FileDescriptor::Open(FL, DELAMETA_HOME_DIRECTORY "/app/README.html", 0));
    auto file_size = TRY(file.file_size());

    res->headers["Content-Length"] = std::to_string(file_size);
    res->headers["Content-Type"] = "text/html";
    file >> res->body_stream;

    return Ok();
}

static HTTP_ROUTE(
    ("/readme", ("GET")),
    (readme), (Ref<http::ResponseWriter>, res, http::arg::response),
    (http::Server::Result<void>)
) {
    auto file = TRY(FileDescriptor::Open(FL, DELAMETA_HOME_DIRECTORY "/app/README.md", 0));
    auto file_size = TRY(file.file_size());

    res->headers["Content-Length"] = std::to_string(file_size);
    res->headers["Content-Type"] = "text/markdown";
    file >> res->body_stream;

    return Ok();
}