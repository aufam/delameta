#include <delameta/http/http.h>
#include <delameta/http/chunked.h>
#include <delameta/utils.h>
#include <gtest/gtest.h>

using namespace Project;
using namespace delameta::http;
using namespace std::literals;
using delameta::Stream;
using delameta::StringStream;
namespace json = delameta::json;

TEST(Http, request) {
    StringStream ss;

    ss.write(
        "POST /submit-form HTTP/1.1\r\n"
        "Host: www.example.com\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 22\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "name=JohnDoe&age=30\r\n"
    );

    RequestReader req(ss, {});
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.url.full_path, "/submit-form");
    EXPECT_EQ(req.version, "HTTP/1.1");
    EXPECT_EQ(req.headers.at("Host"), "www.example.com");
    EXPECT_EQ(req.headers.at("User-Agent"), "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    EXPECT_EQ(req.headers.at("Accept"), "application/json");
    EXPECT_EQ(req.headers.at("Content-Type"), "application/x-www-form-urlencoded");
    EXPECT_EQ(req.headers.at("Content-Length"), "22");
    EXPECT_EQ(req.headers.at("Connection"), "keep-alive");
    EXPECT_EQ(delameta::collect_into<std::string>(req.body_stream.pop_once()), "name=JohnDoe&age=30\r\n");
}

TEST(Http, response) {
    StringStream ss;

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

    ss.write(
        "HTTP/1.1 404 Not Found\r\n"
        "Date: Tue, 20 Aug 2024 12:34:56 GMT\r\n"
        "Server: Apache/2.4.41 (Ubuntu)\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n"
    );

    ss.write(body);

    ResponseReader res(ss, {});
    EXPECT_EQ(res.version, "HTTP/1.1");
    EXPECT_EQ(res.status, 404);
    EXPECT_EQ(res.status_string, "Not Found");
    EXPECT_EQ(res.headers.at("Date"), "Tue, 20 Aug 2024 12:34:56 GMT");
    EXPECT_EQ(res.headers.at("Server"), "Apache/2.4.41 (Ubuntu)");
    EXPECT_EQ(res.headers.at("Content-Type"), "text/html");
    EXPECT_EQ(res.headers.at("Content-Length"), "186");
    EXPECT_EQ(delameta::collect_into<std::string>(res.body_stream.pop_once()), body);
}

TEST(Http, handler) {
    Http handler;

    handler.route("/test", {"POST", "PUT"}).args(arg::method, arg::body, arg::default_val("id", 0))|
    [](std::string_view method, std::string body, int id) {
        auto res = std::string(method) + ": " + body;
        if (id > 0) {
            return res + " with id = " + std::to_string(id);
        }
        return res;
    };

    {
        StringStream ss;
        ss.write("GET /test HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body");
        auto data = ss.read().unwrap();
        auto [req, res] = handler.execute(ss, data);

        EXPECT_EQ(req.method, "GET");
        EXPECT_EQ(req.url.full_path, "/test");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, ""); // req.body is empty because the method constraint is not satisfied

        EXPECT_EQ(res.status, delameta::http::StatusMethodNotAllowed);
        EXPECT_EQ(res.status_string, "Method Not Allowed");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "");
    } {
        StringStream ss;
        ss.write("POST /test HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body");
        auto data = ss.read().unwrap();
        auto [req, res] = handler.execute(ss, data);

        EXPECT_EQ(req.method, "POST");
        EXPECT_EQ(req.url.full_path, "/test");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "this is body");

        EXPECT_EQ(res.status, delameta::http::StatusOK);
        EXPECT_EQ(res.status_string, "OK");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "POST: this is body");
    } {
        StringStream ss;
        ss.write("PUT /test/123 HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body");
        auto data = ss.read().unwrap();
        auto [req, res] = handler.execute(ss, data);

        EXPECT_EQ(req.method, "PUT");
        EXPECT_EQ(req.url.full_path, "/test/123");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "");

        EXPECT_EQ(res.status, delameta::http::StatusNotFound);
        EXPECT_EQ(res.status_string, "Not Found");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "");
    } {
        StringStream ss;
        ss.write("PUT /test?id=123 HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body");
        auto data = ss.read().unwrap();
        auto [req, res] = handler.execute(ss, data);

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

    handler.route("/json", {"POST"}).args(
        arg::json_item("num"),
        arg::json_item("text"),
        arg::json_item("list"),
        arg::json_item("map")
    ) | [](int num, std::string text, json::List list, json::Map map) {
        EXPECT_EQ(num, 42);
        EXPECT_EQ(text, "text");
        EXPECT_EQ(std::get<double>(list[0]), 42);
        EXPECT_EQ(std::get<std::string>(list[1]), "text");
        EXPECT_EQ(std::get<double>(map["num"]), 42);
        EXPECT_EQ(std::get<std::string>(map["text"]), "text");
    };

    std::string body = R"({
        "num": 42,
        "text": "text",
        "list": [42, "text"],
        "map": {
            "num": 42,
            "text": "text",
        }
    })";

    std::string headers =
        "POST /json HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";

    StringStream ss;
    ss.write(headers);
    ss.write(body);

    auto data = ss.read().unwrap();
    auto [req, res] = handler.execute(ss, data);
    EXPECT_EQ(res.status, StatusOK);
}

TEST(Http, form) {
    Http handler;

    handler.route("/form", {"POST"}).args(arg::form("num"), arg::form("text"))|
    [](int num, std::string text) {
        EXPECT_EQ(num, 42);
        EXPECT_EQ(text, "test 123/456-789");
    };

    StringStream ss;
    const std::string body = R"(num=42&text=test+123%2F456-789)";
    ss.write("POST /form HTTP/1.1\r\n");
    ss.write("Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n");
    ss.write("Content-Length: " + std::to_string(body.size()) + "\r\n\r\n");
    ss.write(body);

    auto first_line = ss.read().unwrap();
    auto [req, res] = handler.execute(ss, first_line);
    EXPECT_EQ(res.status, StatusOK);
}

TEST(Http, chunked) {
    Stream s = json::serialize_as_stream(json::Map{
        {"name", std::string("Jupri")},
        {"age", 19},
        {"is_married", true},
        {"salary", 9.1},
        {"role", nullptr},
    });

    s = chunked_encode(s);

    StringStream ss;
    int idx = 0;

    s >> [&](std::string_view sv) {
        ss.write(sv);
        if (idx == 0) { EXPECT_EQ(sv, "10\r\n{\"name\":\"Jupri\",\r\n"sv); }
        if (idx == 1) { EXPECT_EQ(sv, "9\r\n\"age\":19,\r\n"sv); }
        if (idx == 2) { EXPECT_EQ(sv, "12\r\n\"is_married\":true,\r\n"sv); }
        if (idx == 3) { EXPECT_EQ(sv, "F\r\n\"salary\":9.100,\r\n"sv); }
        if (idx == 4) { EXPECT_EQ(sv, "C\r\n\"role\":null}\r\n"sv); }
        if (idx == 5) { EXPECT_EQ(sv, "0\r\n\r\n"sv); }
        ++idx;
    };

    EXPECT_EQ(idx, 6);
    EXPECT_EQ(ss.buffer.size(), 6);

    s = chunked_decode(ss);
    idx = 0;

    s >> [&](std::string_view sv) {
        if (idx == 0) { EXPECT_EQ(sv, "{\"name\":\"Jupri\","sv); }
        if (idx == 1) { EXPECT_EQ(sv, "\"age\":19,"sv); }
        if (idx == 2) { EXPECT_EQ(sv, "\"is_married\":true,"sv); }
        if (idx == 3) { EXPECT_EQ(sv, "\"salary\":9.100,"sv); }
        if (idx == 4) { EXPECT_EQ(sv, "\"role\":null}"sv); }
        if (idx == 5) { EXPECT_EQ(sv, ""sv); }
        ++idx;
    };

    EXPECT_EQ(idx, 6);
    EXPECT_EQ(ss.buffer.size(), 0);
}
