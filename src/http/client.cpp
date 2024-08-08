#include "delameta/http/client.h"

using namespace Project;
using namespace Project::delameta;

using etl::Err;
using etl::Ok;

auto http::request(StreamSessionClient& session, RequestWriter req) -> Result<ResponseReader> {
    if (req.headers.find("User-Agent") == req.headers.end() && 
        req.headers.find("user-agent") == req.headers.end()
    ) {
        req.headers["User-Agent"] = "delameta/" DELAMETA_VERSION;
    }
    if (not req.body.empty() && 
        req.body_stream.rules.empty() &&
        req.headers.find("Content-Length") == req.headers.end() && 
        req.headers.find("content-length") == req.headers.end()
    ) {
        req.headers["Content-Length"] = std::to_string(req.body.size());
    }
    if (req.body.empty() && req.body_stream.rules.empty()) req.headers["Content-Length"] = "0";

    Stream s = req.dump();
    return session.request(s).then([&session](std::vector<uint8_t> data) {
        return http::ResponseReader(*session.desc, std::move(data));
    });
}
