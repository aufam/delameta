#include "delameta/http/server.h"
#include "delameta/file_descriptor.h"
#include <fcntl.h>
#include <dirent.h>
#include <string>

using namespace Project;
using namespace delameta::http;
using delameta::FileDescriptor;
using etl::Ref;
using etl::Err;
using etl::Ok;

struct File {
    FileDescriptor fd;
    size_t size;
};

static Server::Result<File> open_file(const std::string& filename, int flag);
static std::string content_type(const std::string& file);

void file_handler_init(Server& app) {
    app.Get("/ls", std::tuple{arg::arg("path")}, 
    [](std::string path) -> Server::Result<std::list<std::string>> {
        DIR* dir = opendir(path.c_str());
        if (dir == nullptr) {
            return Err(Server::Error{StatusBadRequest, "Error opening " + path});
        }

        struct dirent* entry;
        std::list<std::string> items;
        while ((entry = readdir(dir)) != nullptr) {
            items.emplace_back(entry->d_name);
        }

        closedir(dir);
        return Ok(std::move(items));
    });

    app.Get("/file_size", std::tuple{arg::arg("filename")},
    [](std::string filename) -> Server::Result<std::string> {
        return open_file(filename, O_RDONLY).then([](File file) {
            return std::to_string(file.size) + " bytes";
        });
    });

    static auto read_file = app.Get("/download", std::tuple{arg::arg("filename"), arg::response},
    [](std::string filename, Ref<ResponseWriter> res) -> Server::Result<void> {
        return open_file(filename, O_RDONLY).then([&](File file) {
            res->headers["Content-Length"] = std::to_string(file.size);
            res->headers["Content-Type"] = content_type(filename);
            file.fd >> res->body_stream;
        });
    });

    static auto write_file = app.Put("/upload", std::tuple{arg::arg("filename"), arg::body},
    [](std::string filename, delameta::Stream body_stream) -> Server::Result<void> {
        return open_file(filename, O_WRONLY | O_CREAT | O_TRUNC).then([&](File file) {
            file.fd << body_stream;
        });
    });

    app.Post("/route_file", std::tuple{arg::arg("path"), arg::arg("filename")},
    [&](std::string path, std::string filename) -> Server::Result<void> {
        auto it = std::find_if(app.routers.begin(), app.routers.end(), [&](Server::Router& router) {
            return router.path == path;
        });

        if (it != app.routers.end()) {
            return Err(Server::Error{StatusConflict, "path " + path + " is already exist"});
        }

        app.route(path, {"GET", "PUT"}, std::tuple{arg::method, arg::body, arg::response}, 
        [filename](std::string_view method, delameta::Stream body_stream, Ref<ResponseWriter> res) -> Server::Result<void> {
            if (method == "GET") {
                return read_file(filename, res);
            } else {
                return write_file(filename, std::move(body_stream));
            }
        });

        return Ok();
    });
}

static auto open_file(const std::string& filename, int flag) -> Server::Result<File> {
    auto [stream, stream_err] = FileDescriptor::Open(__FILE__, __LINE__, filename.c_str(), flag);
    if (stream_err) {
        return Err(Server::Error{StatusBadRequest, stream_err->what});
    }

    auto [file_size, file_size_err] = stream->file_size();
    if (file_size_err) {
        return Err(Server::Error{StatusInternalServerError, file_size_err->what});
    }

    return Ok(File{std::move(*stream), *file_size});
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
