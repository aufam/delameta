#include <delameta/http/http.h>
#include <delameta/http/chunked.h>
#include <delameta/file.h>
#include <gtest/gtest.h>
#include <iterator>
#include <string_view>
#include <vector>

using namespace Project;
using namespace delameta::http;
using delameta::Stream;
using etl::Ok;
using etl::Err;
namespace json = delameta::json;

class DummyDescriptor : public delameta::Descriptor {
public:
    delameta::Result<std::vector<uint8_t>> read() override {
        return Err(delameta::Error(-1, "Not implemented"));
    }
    delameta::Result<std::vector<uint8_t>> read_until(size_t) override {
        return Err(delameta::Error(-1, "Not implemented"));
    }
    Stream read_as_stream(size_t) override {
        return {};
    }
    delameta::Result<void> write(std::string_view) override {
        return Err(delameta::Error(-1, "Not implemented"));
    }
};

TEST(Http, request) {
    DummyDescriptor desc;

    std::string payload = 
        "POST /submit-form HTTP/1.1\r\n"
        "Host: www.example.com\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 22\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "name=JohnDoe&age=30\r\n";

    RequestReader req(desc, std::vector<uint8_t>(payload.begin(), payload.end()));
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.url.full_path, "/submit-form");
    EXPECT_EQ(req.version, "HTTP/1.1");
    EXPECT_EQ(req.headers.at("Host"), "www.example.com");
    EXPECT_EQ(req.headers.at("User-Agent"), "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    EXPECT_EQ(req.headers.at("Accept"), "application/json");
    EXPECT_EQ(req.headers.at("Content-Type"), "application/x-www-form-urlencoded");
    EXPECT_EQ(req.headers.at("Content-Length"), "22");
    EXPECT_EQ(req.headers.at("Connection"), "keep-alive");
    EXPECT_EQ(req.body_stream.rules.front()(req.body_stream), "name=JohnDoe&age=30\r\n");
}

TEST(Http, response) {
    DummyDescriptor desc;

    std::string payload = 
        "HTTP/1.1 404 Not Found\r\n"
        "Date: Tue, 20 Aug 2024 12:34:56 GMT\r\n"
        "Server: Apache/2.4.41 (Ubuntu)\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 187\r\n"
        "\r\n";
    
    std::string body =
        "<!DOCTYPE html>\r\n"
        "<html>\r\n"
        "<head>\r\n"
        "    <title>404 Not Found</title>\r\n"
        "</head>\r\n"
        "<body>\r\n"
        "    <h1>Not Found</h1>\r\n"
        "    <p>The requested URL was not found on this server.</p>\r\n"
        "</body>\r\n"
        "</html>\r\n";
    
    payload += body;

    ResponseReader res(desc, std::vector<uint8_t>(payload.begin(), payload.end()));
    EXPECT_EQ(res.version, "HTTP/1.1");
    EXPECT_EQ(res.status, 404);
    EXPECT_EQ(res.status_string, "Not Found");
    EXPECT_EQ(res.headers.at("Date"), "Tue, 20 Aug 2024 12:34:56 GMT");
    EXPECT_EQ(res.headers.at("Server"), "Apache/2.4.41 (Ubuntu)");
    EXPECT_EQ(res.headers.at("Content-Type"), "text/html");
    EXPECT_EQ(res.headers.at("Content-Length"), "187");
    EXPECT_EQ(res.body_stream.rules.front()(res.body_stream), body);
}

TEST(Http, handler) {
    Http handler;
    
    handler.route("/test", {"POST", "PUT"}, std::tuple{arg::method, arg::body, arg::default_val("id", 0)},
    [](std::string_view method, std::string body, int id) {
        auto res = std::string(method) + ": " + body;
        if (id > 0) {
            return res + " with id = " + std::to_string(id);
        }
        return res;
    });

    DummyDescriptor desc;
    {
        std::string payload = "GET /test HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "GET");
        EXPECT_EQ(req.url.full_path, "/test");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, ""); // req.body is empty because the method constraint is not satisfied

        EXPECT_EQ(res.status, delameta::http::StatusMethodNotAllowed);
        EXPECT_EQ(res.status_string, "Method Not Allowed");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "");
    } {
        std::string payload = "POST /test HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "POST");
        EXPECT_EQ(req.url.full_path, "/test");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "this is body");

        EXPECT_EQ(res.status, delameta::http::StatusOK);
        EXPECT_EQ(res.status_string, "OK");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "POST: this is body");
    } {
        std::string payload = "PUT /test/123 HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "PUT");
        EXPECT_EQ(req.url.full_path, "/test/123");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "");

        EXPECT_EQ(res.status, delameta::http::StatusNotFound);
        EXPECT_EQ(res.status_string, "Not Found");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "");
    } {
        std::string payload = "PUT /test?id=123 HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "PUT");
        EXPECT_EQ(req.url.full_path, "/test?id=123");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "this is body");

        EXPECT_EQ(res.status, delameta::http::StatusOK);
        EXPECT_EQ(res.status_string, "OK");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "PUT: this is body with id = 123");
    }
}

TEST(Http, json) {
    Http handler;
    handler.route("/json", {"POST"}, std::tuple{
        arg::json_item("num"), 
        arg::json_item("text"),
        arg::json_item("list"),
        arg::json_item("map"),
    }, [](int num, std::string text, json::List list, json::Map map) {
        EXPECT_EQ(num, 42);
        EXPECT_EQ(text, "text");
        EXPECT_EQ(std::get<double>(list[0]), 42);
        EXPECT_EQ(std::get<std::string>(list[1]), "text");
        EXPECT_EQ(std::get<double>(map["num"]), 42);
        EXPECT_EQ(std::get<std::string>(map["text"]), "text");
    });

    std::string body = R"({
        "num": 42,
        "text": "text",
        "list": [42, "text"],
        "map": {
            "num": 42,
            "text": "text",
        }
    })";

    std::string payload = 
        "POST /json HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
        body;
    
    DummyDescriptor desc;
    const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
    auto [req, res] = handler.execute(desc, payload_vec);
    EXPECT_EQ(res.status, StatusOK);
}

TEST(Http, form) {
    static const std::string body = R"(num=42&text=test+123%2F456-789)";

    class Chunked : public DummyDescriptor {
    public:
        Stream read_as_stream(size_t) override {
            Stream s;
            s << body;
            return s;
        }
    };

    Http handler;
    handler.route("/form", {"POST"}, std::tuple{arg::form("num"), arg::form("text")}, 
    [](int num, std::string text) {
        EXPECT_EQ(num, 42);
        EXPECT_EQ(text, "test 123/456-789");
    });

    std::string header = 
        "POST /form HTTP/1.1\r\n"
        "Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    
    Chunked desc;
    const auto header_vec = std::vector<uint8_t>(header.begin(), header.end());
    auto [req, res] = handler.execute(desc, header_vec);
    EXPECT_EQ(res.status, StatusOK);
}

class StringDescriptor : public delameta::Descriptor {
public:
    explicit StringDescriptor(std::string_view sv) : sv(sv) {}

    delameta::Result<std::vector<uint8_t>> read() override {
        std::vector<uint8_t> res(sv.begin(), sv.end());
        sv = "";
        return Ok(std::move(res));
    }
    delameta::Result<std::vector<uint8_t>> read_until(size_t n) override {
        std::vector<uint8_t> res(sv.begin(), sv.begin() + n);
        sv = sv.substr(n);
        return Ok(std::move(res));
    }
    Stream read_as_stream(size_t) override {
        Stream s;
        s << sv;
        sv = "";
        return s;
    }
    delameta::Result<void> write(std::string_view) override {
        return Err(delameta::Error(-1, "Not implemented"));
    }

    std::string_view sv;
};

TEST(Http, chunked) {
    Stream s = json::serialize_as_stream(json::Map{
        {"name", std::string("Jupri")},
        {"age", 19},
        {"is_married", true},
        {"salary", 9.1},
        {"role", nullptr},
    });

    s = chunked_encode(s);

    std::string buffer;
    int idx = 0;

    s >> [&](std::string_view sv) {
        buffer += sv;
        if (idx == 0) { EXPECT_EQ(sv, std::string_view("10\r\n{\"name\":\"Jupri\",\r\n")); }
        if (idx == 1) { EXPECT_EQ(sv, std::string_view("9\r\n\"age\":19,\r\n")); }
        if (idx == 2) { EXPECT_EQ(sv, std::string_view("12\r\n\"is_married\":true,\r\n")); }
        if (idx == 3) { EXPECT_EQ(sv, std::string_view("F\r\n\"salary\":9.100,\r\n")); }
        if (idx == 4) { EXPECT_EQ(sv, std::string_view("C\r\n\"role\":null}\r\n")); }
        if (idx == 5) { EXPECT_EQ(sv, std::string_view("0\r\n\r\n")); }
        ++idx;
    };

    StringDescriptor sd(buffer);
    Stream ss = chunked_decode(sd);
    idx = 0;

    ss >> [&](std::string_view sv) {
        if (idx == 0) { EXPECT_EQ(sv, std::string_view("{\"name\":\"Jupri\",")); }
        if (idx == 1) { EXPECT_EQ(sv, std::string_view("\"age\":19,")); }
        if (idx == 2) { EXPECT_EQ(sv, std::string_view("\"is_married\":true,")); }
        if (idx == 3) { EXPECT_EQ(sv, std::string_view("\"salary\":9.100,")); }
        if (idx == 4) { EXPECT_EQ(sv, std::string_view("\"role\":null}")); }
        ++idx;
    };
}
