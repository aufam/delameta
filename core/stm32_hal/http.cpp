#include "delameta/http/http.h"

using namespace Project;
using etl::Err;
namespace http = delameta::http;

auto http::Http::Static(std::string path, const std::string& static_dir) -> delameta::Result<void> {
    return Err("Not implemented");
}
