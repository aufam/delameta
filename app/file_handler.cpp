#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/file.h>
#include <algorithm>
#include <dirent.h>

using namespace Project;
namespace http = delameta::http;
using delameta::File;
using delameta::Stream;
using etl::Ref;
using etl::Err;
using etl::Ok;

static std::string content_type(const std::string& file);

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/ls", ("GET")),
    (ls), (std::string, path, http::arg::arg("path")),
    (http::Result<std::list<std::string>>)
) {
    DIR* dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        return Err(http::Error{http::StatusBadRequest, "Error opening " + path});
    }

    struct dirent* entry;
    std::list<std::string> items;
    while ((entry = ::readdir(dir)) != nullptr) {
        items.emplace_back(entry->d_name);
    }

    closedir(dir);
    return Ok(std::move(items));
}

static HTTP_ROUTE(
    ("/file_size", ("GET")),
    (file_size), (std::string, filename, http::arg::arg("filename")),
    (http::Result<std::string>)
) {
    return File::Open(FL, {filename}).then([](File file) {
        return std::to_string(file.file_size()) + " bytes";
    });
}

static HTTP_ROUTE(
    ("/download", ("GET")),
    (download), 
        (std::string        , filename, http::arg::arg("filename"))
        (Ref<http::ResponseWriter>, res     , http::arg::response       ),
    (http::Result<void>)
) {
    return File::Open(FL, {filename}).then([&](File file) {
        res->headers["Content-Length"] = std::to_string(file.file_size());
        res->headers["Content-Type"] = content_type(filename);
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
    return File::Open(FL, {filename, "w"}).then([&](File file) {
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
    auto it = std::find_if(app.routers.begin(), app.routers.end(), [&](http::Router& router) {
        return router.path == path;
    });

    if (it != app.routers.end()) {
        return Err(http::Error{http::StatusConflict, "path " + path + " is already exist"});
    }

    app.route(path, {"GET", "PUT"}, std::tuple{http::arg::method, http::arg::body, http::arg::response}, 
    [filename](std::string_view method, Stream body_stream, Ref<http::ResponseWriter> res) -> http::Result<void> {
        if (method == "GET") {
            return download(filename, res);
        } else {
            return upload(filename, std::move(body_stream));
        }
    });

    return Ok();
}

static std::string content_type(const std::string& file) {
    std::string extension = file.substr(file.find_last_of('.') + 1);
    return 
        extension == "js"   ? "application/javascript" :
        extension == "json" ? "application/json" :
        extension == "pdf"  ? "application/pdf" :
        extension == "xml"  ? "application/xml" :
        extension == "css"  ? "text/css" :
        extension == "html" ? "text/html" :
        extension == "txt"  ? "text/plain" :
        extension == "jpeg" ? "image/jpeg" :
        extension == "jpg"  ? "image/jpeg" :
        extension == "png"  ? "image/png" :
        extension == "gif"  ? "image/gif" :
        extension == "mp4"  ? "video/mp4" :
        extension == "mpeg" ? "audio/mpeg" :
        extension == "mp3"  ? "audio/mpeg" :
        extension == "wav"  ? "audio/wav" :
        extension == "ogg"  ? "audio/ogg" :
        extension == "flac" ? "audio/flac" :
        extension == "avi"  ? "video/x-msvideo" :
        extension == "mov"  ? "video/quicktime" :
        extension == "webm" ? "video/webm" :
        extension == "mkv"  ? "video/x-matroska" :
        extension == "zip"  ? "application/zip" :
        extension == "rar"  ? "application/x-rar-compressed" :
        extension == "tar"  ? "application/x-tar" :
        extension == "gz"   ? "application/gzip" :
        extension == "doc"  ? "application/msword" :
        extension == "docx" ? "application/vnd.openxmlformats-officedocument.wordprocessingml.document" :
        extension == "ppt"  ? "application/vnd.ms-powerpoint" :
        extension == "pptx" ? "application/vnd.openxmlformats-officedocument.presentationml.presentation" :
        extension == "xls"  ? "application/vnd.ms-excel" :
        extension == "xlsx" ? "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" :
        extension == "odt"  ? "application/vnd.oasis.opendocument.text" :
        extension == "ods"  ? "application/vnd.oasis.opendocument.spreadsheet" :
        extension == "svg"  ? "image/svg+xml" :
        extension == "ico"  ? "image/x-icon" :
        extension == "md"   ? "text/markdown" :
        "application/octet-stream" // Default to binary data
    ;
}
