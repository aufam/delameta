#include <boost/preprocessor.hpp>
#include <fmt/format.h>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/file.h>
#include <delameta/utils.h>
#include <filesystem>

using namespace Project;
using delameta::File;
using delameta::Stream;
using etl::Ref;
using etl::Err;
using etl::Ok;
namespace http = delameta::http;
namespace fs = std::filesystem;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/ls", ("GET")),
    (ls), (std::string, path, http::arg::arg("path")),
    (http::Result<std::list<std::string>>)
) {
    fs::path dir = path;
    if (not fs::is_directory(dir)) {
        return Err(http::Error{http::StatusBadRequest, fmt::format("`{}` is not a directory", path)});
    }

    std::list<std::string> items;
    for (const auto& entry: fs::directory_iterator(dir)) {
        items.emplace_back(fs::relative(entry.path(), dir).string());
    }

    return Ok(std::move(items));
}

static HTTP_ROUTE(
    ("/file_size", ("GET")),
    (file_size), (std::string, filename, http::arg::arg("filename")),
    (http::Result<size_t>)
) {
    return File::Open(FL, File::Args{filename}).then([](File file) {
        return file.file_size();
    });
}

static HTTP_ROUTE(
    ("/download", ("GET")),
    (download),
        (std::string              , filename, http::arg::arg("filename"))
        (Ref<http::ResponseWriter>, res     , http::arg::response       ),
    (http::Result<void>)
) {
    return File::Open(FL, File::Args{filename}).then([&](File file) {
        res->headers["Content-Type"] = delameta::get_content_type_from_file(filename);
        file >> res->body_stream;
    });
}

static HTTP_ROUTE(
    ("/upload", ("PUT")),
    (upload),
        (std::string, filename   , http::arg::arg("filename"))
        (Stream     , body_stream, http::arg::body           ),
    (http::Result<void>)
) {
    return File::Open(FL, File::Args{filename, "w"}).then([&](File file) {
        file << body_stream;
    });
}

static HTTP_ROUTE(
    ("/route_file", ("POST")),
    (route_file),
        (std::string, path    , http::arg::arg("path"))
        (std::string, filename, http::arg::arg("filename")),
    (http::Result<void>)
) {
    if (app.routers.find(path) != app.routers.end()) {
        return Err(http::Error{http::StatusConflict, "path " + path + " is already exist"});
    }

    app.route(path, {"GET", "PUT"}).args(http::arg::method, http::arg::body, http::arg::response)|
    [filename](std::string_view method, Stream body_stream, Ref<http::ResponseWriter> res) -> http::Result<void> {
        if (method == "GET") {
            return download(filename, res);
        } else {
            return upload(filename, std::move(body_stream));
        }
    };

    return Ok();
}
