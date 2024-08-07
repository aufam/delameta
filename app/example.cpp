#include "delameta/http/server.h"
#include "delameta/http/client.h"
#include "debug.ipp"

using namespace Project;
using namespace Project::delameta::http;
using delameta::Stream;
using delameta::URL;
using delameta::Error;
using delameta::info;
using etl::Ref;
using etl::Ok;
using etl::Err;

// define some json rules for some http classes
JSON_DEFINE(Project::delameta::http::Server::Router, 
    JSON_ITEM("methods", methods), 
    JSON_ITEM("path", path)
)

JSON_DEFINE(Project::delameta::http::Server::Error, 
    JSON_ITEM("err", what)
)

JSON_DEFINE(Project::delameta::URL, 
    JSON_ITEM("url", url), 
    JSON_ITEM("protocol", protocol), 
    JSON_ITEM("host", host), 
    JSON_ITEM("path", path), 
    JSON_ITEM("full_path", full_path), 
    JSON_ITEM("queries", queries),
    JSON_ITEM("fragment", fragment)
)

// example custom struct with json
struct Foo {
    int num;
    std::string text;
};

JSON_DEFINE(Foo, 
    JSON_ITEM("num", num), 
    JSON_ITEM("text", text)
)

// example custom struct with custom serialization/deserialization and response writer
struct Bar {
    size_t num;
};

template<>
auto Server::convert_string_into(std::string_view str) -> Server::Result<Bar> {
    return Ok(Bar{str.size()});
}

template<>
void Server::process_result(Bar& bar, const RequestReader&, ResponseWriter& res) {
    res.body = "Bar{" + std::to_string(bar.num) + "}";
    res.headers["Content-Type"] = "text/plain";
}

// example JWT dependency injection
static const char* const access_token = "Bearer 1234";

static auto get_token(const RequestReader& req, ResponseWriter&) -> Server::Result<std::string_view> {
    std::string_view token = "";
    auto it = req.headers.find("Authentication");
    if (it == req.headers.end()) {
        it = req.headers.find("authentication");
    } 
    if (it != req.headers.end()) {
        token = it->second;
    } else {
        return Err(Server::Error{StatusUnauthorized, "No authentication provided"});
    }
    if (token == access_token) {
        return Ok(token);
    } else {
        return Err(Server::Error{StatusUnauthorized, "Token doesn't match"});
    }
};

void example_init(Server& app) {
    // show response time in the response header
    app.show_response_time = true;

    app.logger = [](const std::string& ip, const RequestReader& req, const ResponseWriter& res) {
        std::string msg = ip + " " + std::string(req.method) + " " + req.url.path + " " + std::to_string(res.status) + " " + res.status_string;
        info(__FILE__, __LINE__, msg);
    };

    // example custom handler: jsonify Server::Error
    app.error_handler = [](Server::Error err, const RequestReader&, ResponseWriter& res) {
        res.status = err.status;
        res.body = etl::json::serialize(err);
        res.headers["Content-Type"] = "application/json";
    };

    // example: print hello
    app.Get("/hello", {}, 
    []() {
        return "Hello world from delameta/" DELAMETA_VERSION;
    });

    // example: print hello with msg
    app.Post("/hello", std::tuple{arg::json_item("msg")}, 
    [](std::string msg) {
        return "Hello world " + msg;
    });

    // example: 
    // - get request param (in this case the body as string_view)
    // - possible error return value
    app.Post("/body", std::tuple{arg::body},
    [](std::string_view body) -> Server::Result<std::string_view> {
        if (body.empty()) {
            return etl::Err(Server::Error{StatusBadRequest, "Body is empty"});
        } else {
            return etl::Ok(body);
        }
    });

    // example: 
    // - dependency injection (in this case is authentication token), 
    // - arg with default value, it will try to find "add" key in the request headers and request queries
    //   If not found, use the default value
    // - since json rule for Foo is defined and the arg type is json, the request body will be deserialized into foo
    // - new foo will be created and serialized as response body
    app.Post("/foo", std::tuple{arg::depends(get_token), arg::default_val("add", 20), arg::json},
    [](std::string_view token, int add, Foo foo) -> Foo {
        return {foo.num + add, foo.text + ": " + std::string(token)}; 
    });

    // example: 
    // - it will find "bar" key in the request headers and request queries
    // - if found, it will be deserialized using custom Server::convert_string_into
    // - since Server::process_result for Bar is defined, the return value of this function will be used to process the result 
    app.Get("/bar", std::tuple{arg::arg("bar")}, 
    [](Bar bar) -> Bar { 
        return bar; 
    });

    // example:
    // multiple methods handler
    app.route("/methods", {"GET", "POST"}, std::tuple{arg::method},
    [](std::string_view method) {
        if (method == "GET") {
            return "Example GET method";
        } else {
            return "Example POST method";
        }
    });

    // example: print all routes of this app as json list
    app.Get("/routes", {},
    [&]() -> Ref<const std::list<Server::Router>> {
        return etl::ref_const(app.routers);
    });

    // example: print all headers
    app.Get("/headers", std::tuple{arg::headers},
    [](Ref<const decltype(RequestReader::headers)> headers) {
        return headers;
    });

    // example: print all queries
    app.Get("/queries", std::tuple{arg::queries},
    [](Ref<const decltype(URL::queries)> queries) {
        return queries;
    });

    // example: print url
    app.Get("/url", std::tuple{arg::url},
    [](Ref<const URL> url) {
        return url;
    });

    // example: print url
    app.Get("/resolve_url", std::tuple{arg::arg("url")},
    [](std::string url) {
        return URL(url);
    });

    // example: redirect to the given path
    app.route("/redirect", {"GET", "POST", "PUT", "PATCH", "HEAD", "TRACE", "DELETE", "OPTIONS"}, 
    std::tuple{arg::request, arg::arg("url")}, 
    [](Ref<const RequestReader> req, std::string url_str) -> Server::Result<ResponseReader> {
        URL url = url_str;
        return Client::New(__FILE__, __LINE__, {url.host}).and_then([&](Client cli) {
            RequestWriter request = *req;
            request.url = url;
            return cli.request(std::move(request));
        })
        .except([](Error err) {
            return Server::Error{StatusInternalServerError, err.what};
        });
    });
}
