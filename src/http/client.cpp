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

    Stream s = req.dump();
    return tcp::Client::request(s).then([this](std::vector<uint8_t> data) {
        return http::ResponseReader(*socket, std::move(data));
    });
}

auto http::Client::New(const char* file, int line, Args args) -> Result<http::Client> {
    return tcp::Client::New(file, line, args).then([](tcp::Client cli) {
        return http::Client(std::move(cli));
    });
}

http::Client::Client(tcp::Client&& other) : tcp::Client(std::move(other)) {}