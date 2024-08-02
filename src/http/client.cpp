#include "delameta/http/client.h"

using namespace Project;
using namespace Project::delameta;

using etl::Err;
using etl::Ok;

auto http::Client::request(http::RequestWriter req) -> Result<http::ResponseReader> {
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

    return tcp::Client::request(req.dump()).then([this](std::vector<uint8_t> data) {
        return http::ResponseReader(*stream, std::move(data));
    });
}

auto http::Client::New(const char* file, int line, Args args) -> Result<http::Client> {
    auto [client, err] = tcp::Client::New(file, line, args);
    if (err) {
        return Err(std::move(*err));
    } else {
        return Ok(http::Client(std::move(*client)));
    }
}

http::Client::Client(tcp::Client&& other) : tcp::Client(std::move(other)) {}