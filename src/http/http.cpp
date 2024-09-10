#include "delameta/http/http.h"
#include "delameta/tcp.h"
#include "delameta/tls.h"
#include <algorithm>
#include "../time_helper.ipp"

using namespace Project;
using namespace Project::delameta;
using etl::Err;
using etl::Ok;

http::Error::Error(int status) : status(status), what("") {}
http::Error::Error(int status, std::string what) : status(status), what(std::move(what)) {}
http::Error::Error(delameta::Error err) : status(StatusInternalServerError), what(err.what + ": " + std::to_string(err.code)) {}

auto http::request(StreamSessionClient& session, RequestWriter req) -> delameta::Result<ResponseReader> {
    if (req.headers.find("User-Agent") == req.headers.end() && 
        req.headers.find("user-agent") == req.headers.end()
    ) {
        req.headers["User-Agent"] = "delameta/" DELAMETA_VERSION;
    }
    if (req.headers.find("Host") == req.headers.end() && 
        req.headers.find("host") == req.headers.end()
    ) {
        req.headers["Host"] = req.url.host;
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

auto http::request(StreamSessionClient&& session, RequestWriter req) -> delameta::Result<ResponseReader> {
    if (req.headers.find("User-Agent") == req.headers.end() && 
        req.headers.find("user-agent") == req.headers.end()
    ) {
        req.headers["User-Agent"] = "delameta/" DELAMETA_VERSION;
    }
    if (req.headers.find("Host") == req.headers.end() && 
        req.headers.find("host") == req.headers.end()
    ) {
        req.headers["Host"] = req.url.host;
    }
    if (req.headers.find("Connection") == req.headers.end() && 
        req.headers.find("connection") == req.headers.end()
    ) {
        req.headers["Connection"] = "close";
    }

    if (not req.body.empty() && 
        req.body_stream.rules.empty() &&
        req.headers.find("Content-Length") == req.headers.end() && 
        req.headers.find("content-length") == req.headers.end()
    ) {
        req.headers["Content-Length"] = std::to_string(req.body.size());
    }
    if (req.body.empty() && req.body_stream.rules.empty()) req.headers["Content-Length"] = "0";

    auto session_ptr = new StreamSessionClient(std::move(session));

    Stream s = req.dump();
    return session_ptr->request(s).then([session_ptr](std::vector<uint8_t> data) {
        auto res = http::ResponseReader(*session_ptr->desc, std::move(data));
        res.body_stream.at_destructor = [session_ptr]() { delete session_ptr; };
        return res;
    }).except([session_ptr](auto e) {
        delete session_ptr;
        return e;
    });
}

auto http::request(RequestWriter req) -> delameta::Result<ResponseReader> {
    if (req.url.url.substr(0, 8) == "https://") {
        auto [session, err] = TLS::Open(__FILE__, __LINE__, TLS::Args{.host=req.url.url});
        if (err) return Err(std::move(*err));
        return request(StreamSessionClient(new TLS(std::move(*session))), std::move(req));
    } else {
        auto [session, err] = TCP::Open(__FILE__, __LINE__, TCP::Args{.host=req.url.url});
        if (err) return Err(std::move(*err));
        return request(StreamSessionClient(new TCP(std::move(*session))), std::move(req));
    }
}

auto http::Http::reroute(std::string path, etl::Ref<const RequestReader> req, etl::Ref<ResponseWriter> res) -> Result<void> {
    auto it = std::find_if(routers.begin(), routers.end(), [&](http::Router& router) {
        return router.path == path;
    });

    if (it == routers.end()) {
        return Err(Error{StatusNotFound, "path " + path + " is not found"});
    }

    it->function(*req, *res);
    return Ok();
}

void http::Http::bind(StreamSessionServer& server, BindArg is_tcp_server) const {
    server.handler = [this, is_tcp_server](Descriptor& desc, const std::string& name, const std::vector<uint8_t>& data) -> Stream {
        auto [req, res] = execute(desc, data);

        // handle socket configuration
        if (is_tcp_server.is_tcp_server) {
            if (auto socket = static_cast<TCP*>(&desc); socket) {
                auto it = req.headers.find("Connection");
                if (it == req.headers.end()) {
                    it = req.headers.find("connection");
                }
                if (it != req.headers.end()) {
                    if (it->second == "keep-alive") {
                        socket->keep_alive = true;
                    } else if (it->second == "close") {
                        socket->keep_alive = false;
                    }
                }

                // handle keep alive
                it = req.headers.find("Keep-Alive");
                if (it == req.headers.end()) {
                    it = req.headers.find("keep-alive");
                }
                if (it != req.headers.end()) {
                    std::string_view value = it->second;
                    auto timeout_idx = value.find("timeout=");
                    if (timeout_idx < value.size()) {
                        socket->timeout = ::atoi(value.data() + timeout_idx + 9);
                    }
                    auto max_idx = value.find("max=");
                    if (max_idx < value.size()) {
                        socket->max = ::atoi(value.data() + max_idx + 5);
                    }
                }
            }
        }

        if (logger) logger(name, req, res);
        return res.dump();
    };
}

auto http::Http::execute(Descriptor& desc, const std::vector<uint8_t>& data) const -> std::pair<RequestReader, ResponseWriter> {
    auto start = delameta_detail_get_time_stamp();
    auto req = http::RequestReader(desc, data);
    auto res = http::ResponseWriter{};
    
    // TODO: version handling
    res.version = req.version;

    // router handler
    bool handled = false;
    for (auto &router : routers) {
        if (router.path != req.url.path) continue;
        auto it = std::find(router.methods.begin(), router.methods.end(), req.method);
        handled = true;
        if (it == router.methods.end()) {
            res.status = StatusMethodNotAllowed;
        } else {
            res.status = StatusOK;
            router.function(req, res);
            break;
        }
    }

    if (not handled) res.status = StatusNotFound;
    if (res.status_string.empty()) res.status_string = status_to_string(res.status);
    if (res.headers.find("Server") == res.headers.end() && 
        res.headers.find("server") == res.headers.end()
    ) {
        res.headers["Server"] = "delameta/" DELAMETA_VERSION;
    }
    if (not res.body.empty() && 
        res.body_stream.rules.empty() &&
        res.headers.find("Content-Length") == res.headers.end() && 
        res.headers.find("content-length") == res.headers.end()
    ) {
        res.headers["Content-Length"] = std::to_string(res.body.size());
    }
    if (res.body.empty() && res.body_stream.rules.empty()) res.headers["Content-Length"] = "0";

    for (auto &[key, fn] : global_headers) {
        auto value = fn(req, res);
        if (not value.empty()) res.headers[key] = std::move(value);
    }

    auto elapsed_ms = delameta_detail_count_ms(start);
    if (show_response_time) res.headers["X-Response-Time"] = std::to_string(elapsed_ms) + "ms";

    return {std::move(req), std::move(res)};
}

http::Http::Context::Context(const RequestReader& req) {
    auto it = req.headers.find("Content-Type");
    if (it == req.headers.end()) {
        it = req.headers.find("content-type");
    }
    if (it != req.headers.end()) {
        content_type = it->second;
    }

    if (content_type_starts_with("application/json")) {
        if (req.body.empty()) req.body_stream >> [&req](std::string_view chunk) { req.body += chunk; };
        json = etl::Json::parse(etl::string_view(req.body.data(), req.body.size()));
        type = JSON;
    } else if (content_type_starts_with("application/x-www-form-urlencoded")) {
        if (req.body.empty()) req.body_stream >> [&req](std::string_view chunk) { req.body += chunk; };
        form = URL::decode(req.body);
        type = Form;
    }
}

bool http::Http::Context::content_type_starts_with(std::string_view prefix) const {
    return content_type.substr(0, prefix.length()) == prefix;
}

auto http::Http::Context::form_at(const char* key) const -> http::Result<std::string_view> {
    const std::string k = key;
    auto it = form.find(k);
    if (it == form.end()) return etl::Err(Error{StatusBadRequest, "key '" + k + "' not found"});
    return etl::Ok(std::string_view(it->second));
}
