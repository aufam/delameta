#include "delameta/http/http.h"

using namespace Project;
using etl::Err;
namespace http = delameta::http;

auto http::Http::Static(const std::string&, const std::string&, bool) -> delameta::Result<void> {
    return Err("Not implemented");
}
