#include "delameta/http/http.h"
#include "delameta/http/chunked.h"
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

static void setup_request(http::RequestWriter& req) {
    if (req.headers.find("User-Agent") == req.headers.end() && 
        req.headers.find("user-agent") == req.headers.end()
    ) {
        req.headers.emplace("User-Agent", "delameta/" DELAMETA_VERSION);
    }
    if (req.headers.find("Host") == req.headers.end() && 
        req.headers.find("host") == req.headers.end()
    ) {
        req.headers.emplace("Host", req.url.host);
    }

    auto content_length_it = req.headers.find("Content-Length");
    if (content_length_it == req.headers.end()) content_length_it = req.headers.find("content-length");
    bool content_length_found = content_length_it != req.headers.end();

    auto set_content_length = [&](size_t n, bool force) {
        if (content_length_found) {
            if (force) content_length_it->second = std::to_string(n);
        } else {
            req.headers.emplace("Content-Length", std::to_string(n));
        }
    };

    if (!req.body.empty()) {
        set_content_length(req.body.size(), false);
    } else if (!req.body_stream.rules.empty()) {
        if (!content_length_found) {
            auto transfer_encoding_it = req.headers.find("Transfer-Encoding");
            if (transfer_encoding_it == req.headers.end()) transfer_encoding_it = req.headers.find("transfer-encoding");
            bool transfer_encoding_found = transfer_encoding_it != req.headers.end();

            if (!transfer_encoding_found) {
                req.body_stream = http::chunked_encode(req.body_stream);
                req.headers.emplace("Transfer-Encoding", "chunked");
            }
        }
    } else {
        if (!(req.method == "GET" or req.method == "DELETE" or req.method == "HEAD" or req.method == "OPTIONS")) {
            set_content_length(0, true);
        }
    }
}

auto http::request(StreamSessionClient& session, RequestWriter req) -> delameta::Result<ResponseReader> {
    if (!req.body.empty() && !req.body_stream.rules.empty()) {
        return Err("Multiple body source");
    }

    setup_request(req);

    Stream s = req.dump();
    return session.request(s).then([&session](std::vector<uint8_t> data) {
        return http::ResponseReader(*session.desc, std::move(data));
    });
}

auto http::request(StreamSessionClient&& session, RequestWriter req) -> delameta::Result<ResponseReader> {
    if (!req.body.empty() && !req.body_stream.rules.empty()) {
        return Err("Multiple body source");
    }

    setup_request(req);

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
    if (req.url.url.size() >= 8 && req.url.url.substr(0, 8) == "https://") {
        auto [session, err] = TLS::Open(__FILE__, __LINE__, TLS::Args{
            .host=req.url.url,
        });
        if (err) return Err(std::move(*err));
        return request(StreamSessionClient(new TLS(std::move(*session))), std::move(req));
    } else {
        auto [session, err] = TCP::Open(__FILE__, __LINE__, TCP::Args{.host=req.url.url});
        if (err) return Err(std::move(*err));
        return request(StreamSessionClient(new TCP(std::move(*session))), std::move(req));
    }
}

auto http::Http::reroute(std::string path, etl::Ref<const RequestReader> req, etl::Ref<ResponseWriter> res) -> Result<void> {
    auto [begin, end] = routers.equal_range(path);

    for (auto it = begin; it != end; ++it) {
        auto m = std::find_if(it->second.methods.begin(), it->second.methods.end(), [&](const char* method) {
            return req->method == method;
        });

        if (m == it->second.methods.end()) continue;

        it->second.function(*req, *res);
        return Ok();
    };

    return Err(Error{StatusNotFound, "path " + path + " is not found"});
}

void http::Http::execute(const http::RequestReader& req, http::ResponseWriter& res) const {
    auto start = delameta_detail_get_time_stamp();

    // TODO: version handling
    res.version = req.version;

    // router handler
    auto [begin, end] = routers.equal_range(req.url.path);
    bool handled = false;

    for (auto it = begin; it != end; ++it) {
        handled = true;
        auto &router = it->second;
        auto m = std::find(router.methods.begin(), router.methods.end(), req.method);

        if (m == router.methods.end()) {
            res.status = StatusMethodNotAllowed;
            continue; // try next iterator
        }

        res.status = StatusOK;
        router.function(req, res);
        break;
    };

    if (not handled) error_handler(Error(StatusNotFound), req, res);

    if (!res.body.empty() && !res.body_stream.rules.empty()) {
        error_handler(Error{StatusInternalServerError, "Multiple body sources"}, req, res);
    }

    auto content_length_it = res.headers.find("Content-Length");
    if (content_length_it == res.headers.end()) content_length_it = res.headers.find("content-length");
    bool content_length_found = content_length_it != res.headers.end();

    auto set_content_length = [&](size_t n, bool force) {
        if (content_length_found) {
            if (force) content_length_it->second = std::to_string(n);
        } else {
            res.headers.emplace("Content-Length", std::to_string(n));
        }
    };

    if (!res.body.empty() && !res.body_stream.rules.empty()) {
    } else if (!res.body.empty()) {
        set_content_length(res.body.size(), false);
    } else if (!res.body_stream.rules.empty()) {
        if (!content_length_found) {
            auto transfer_encoding_it = res.headers.find("Transfer-Encoding");
            if (transfer_encoding_it == res.headers.end()) transfer_encoding_it = res.headers.find("transfer-encoding");
            bool transfer_encoding_found = transfer_encoding_it != res.headers.end();

            if (!transfer_encoding_found) {
                res.body_stream = http::chunked_encode(res.body_stream);
                res.headers.emplace("Transfer-Encoding", "chunked");
            }
        }
    } else {
        set_content_length(0, true);
    }

    for (auto &[key, fn] : global_headers) {
        auto value = fn(req, res);
        if (not value.empty()) res.headers[key] = std::move(value);
    }

    if (res.headers.find("Server") == res.headers.end() && 
        res.headers.find("server") == res.headers.end()
    ) {
        res.headers.emplace("Server", "delameta/" DELAMETA_VERSION);
    }

    if (res.status_string.empty()) res.status_string = status_to_string(res.status);

    auto elapsed_ms = delameta_detail_count_ms(start);
    if (show_response_time) res.headers["X-Response-Time"] = std::to_string(elapsed_ms) + "ms";
}

auto http::Http::execute(Descriptor& desc) const -> std::pair<RequestReader, ResponseWriter> {
    auto read_result = desc.read();
    if (read_result.is_err()) {
        return {};
    }

    auto req = http::RequestReader(desc, std::move(read_result.unwrap()));
    auto res = http::ResponseWriter{};

    execute(req, res);
    return {std::move(req), std::move(res)};
}

auto http::Http::execute(Descriptor& desc, std::vector<uint8_t>& data) const -> std::pair<RequestReader, ResponseWriter> {
    auto req = http::RequestReader(desc, data);
    auto res = http::ResponseWriter{};

    execute(req, res);
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
    return content_type.length() >= prefix.length() && content_type.substr(0, prefix.length()) == prefix;
}

auto http::Http::Context::form_at(const char* key) const -> Result<std::string_view> {
    const std::string k = key;
    auto it = form.find(k);
    if (it == form.end()) return Err(Error{StatusBadRequest, "key '" + k + "' not found"});
    return Ok(std::string_view(it->second));
}

void http::Http::bind(StreamSessionServer& server, BindArg is_tcp_server) const {
    server.handler = [this, is_tcp_server](Descriptor& desc, const std::string& name, std::vector<uint8_t>& data) -> Stream {
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

auto http::Http::listen(http::Http::ListenArgs args) const -> delameta::Result<void> {
        Server<TCP> svr;
        delameta_detail_on_sigint([&]() { svr.stop(); });
        return svr.start(__FILE__, __LINE__, Server<TCP>::Args{
            .host=args.host,
            .max_socket=args.max_socket,
            .keep_alive=args.keep_alive,
            .timeout=args.timeout
        });
    if (args.cert_file.empty()) {
        Server<TCP> svr;
        delameta_detail_on_sigint([&]() { svr.stop(); });
        return svr.start(__FILE__, __LINE__, Server<TCP>::Args{
            .host=args.host,
            .max_socket=args.max_socket,
            .keep_alive=args.keep_alive,
            .timeout=args.timeout
        });
    } else {
        Server<TLS> svr;
        delameta_detail_on_sigint([&]() { svr.stop(); });
        return svr.start(__FILE__, __LINE__, Server<TLS>::Args{
            .host=args.host,
            .cert_file=args.cert_file,
            .key_file=args.key_file,
            .max_socket=args.max_socket,
            .keep_alive=args.keep_alive,
            .timeout=args.timeout
        });
    }
}
