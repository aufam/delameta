#include <delameta/http/server.h>
#include <gtest/gtest.h>

using namespace Project;
using namespace delameta::http;
using delameta::Result;
using delameta::Stream;
using etl::Ok;
using etl::Err;

class DummyDescriptor : public delameta::Descriptor {
public:
    Result<std::vector<uint8_t>> read() override {
        return Err(delameta::Error(-1, "Not implemented"));
    }
    Result<std::vector<uint8_t>> read_until(size_t) override {
        return Err(delameta::Error(-1, "Not implemented"));
    }
    Stream read_as_stream(size_t) override {
        return {};
    }
    Result<void> write(std::string_view) override {
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
    EXPECT_EQ(req.body_stream.rules.front()(), "name=JohnDoe&age=30\r\n");
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
    EXPECT_EQ(res.body_stream.rules.front()(), body);
}

TEST(Http, handler) {
    Server handler;
    
    handler.route("/test", {"GET"}, std::tuple{arg::body, arg::default_val("id", 0)},
    [](std::string body, int id) {
        if (id > 0) return body + " with id = " + std::to_string(id);
        return body;
    });

    DummyDescriptor desc;
    {
        std::string payload = "GET /test HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "GET");
        EXPECT_EQ(req.url.full_path, "/test");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "this is body");

        EXPECT_EQ(res.status, delameta::http::StatusOK);
        EXPECT_EQ(res.status_string, "OK");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "this is body");
    } {
        std::string payload = "POST /test HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "POST");
        EXPECT_EQ(req.url.full_path, "/test");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "");

        EXPECT_EQ(res.status, delameta::http::StatusMethodNotAllowed);
        EXPECT_EQ(res.status_string, "Method Not Allowed");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "");
    } {
        std::string payload = "GET /test123 HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "GET");
        EXPECT_EQ(req.url.full_path, "/test123");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "");

        EXPECT_EQ(res.status, delameta::http::StatusNotFound);
        EXPECT_EQ(res.status_string, "Not Found");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "");
    } {
        std::string payload = "GET /test?id=123 HTTP/1.1\r\nContent-Length: 12\r\n\r\nthis is body";
        const auto payload_vec = std::vector<uint8_t>(payload.begin(), payload.end());
        auto [req, res] = handler.execute(desc, payload_vec);

        EXPECT_EQ(req.method, "GET");
        EXPECT_EQ(req.url.full_path, "/test?id=123");
        EXPECT_EQ(req.version, "HTTP/1.1");
        EXPECT_EQ(req.body, "this is body");

        EXPECT_EQ(res.status, delameta::http::StatusOK);
        EXPECT_EQ(res.status_string, "OK");
        EXPECT_EQ(res.version, "HTTP/1.1");
        EXPECT_EQ(res.body, "this is body with id = 123");
    }
}