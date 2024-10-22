#include "delameta/http/http.h"
#include "delameta/file.h"
#include "delameta/utils.h"
#include <filesystem>

using namespace Project;
using etl::Ok;
using etl::Err;
using etl::Ref;
using delameta::File;
namespace fs = std::filesystem;
namespace http = delameta::http;

static auto get_file(const std::string& filename, Ref<http::ResponseWriter> res, bool chunked) -> http::Result<void> {
    return File::Open(File::Args{.filename=filename, .mode="r"}).then([&](File file) {
        res->headers["Content-Type"] = delameta::get_content_type_from_file(filename);
        if (not chunked) {
            res->headers["Content-Lenght"] = std::to_string(file.file_size());
        }
        file >> res->body_stream;
    });
}

auto http::Http::Static(const std::string& prefix, const std::string& root, bool chunked) -> delameta::Result<void> {
    fs::path dir = root;

    if (not fs::is_directory(dir)) {
        return Err(Error{-1, root + " is not a directory"});
    }

    dir = fs::absolute(dir);
    for (const auto& entry: fs::recursive_directory_iterator(dir)) {
        fs::path abs_file = entry.path();
        fs::path rel_file = fs::relative(abs_file, dir);
        std::string route_name = prefix + rel_file.string();

        this->Get(route_name).args(http::arg::response)|
        [filename=abs_file.string(), chunked](Ref<ResponseWriter> res) {
            return get_file(filename, res, chunked);
        };

        if (route_name == "/index.html") {
            this->Get(prefix).args(http::arg::request, http::arg::response)|
            [this](Ref<const http::RequestReader> req, Ref<http::ResponseWriter> res) {
                return this->reroute("/index.html", req, res);
            };
        }
    }

    return Ok();
}
