#include <boost/preprocessor.hpp>
#include <fmt/format.h>
#include <delameta/url.h>
#include <delameta/modbus/api.h>
#include <gtest/gtest.h>

using namespace Project;
using delameta::URL;

TEST(URL, plain) {
    URL plain("https://example.com");

    EXPECT_EQ(plain.url, "https://example.com");
    EXPECT_EQ(plain.protocol, "https");
    EXPECT_EQ(plain.host, "example.com");
}

TEST(URL, port) {
    URL with_port("https://example.com:8080");

    EXPECT_EQ(with_port.protocol, "https");
    EXPECT_EQ(with_port.host, "example.com:8080");
}

TEST(URL, path) {
    URL with_path("https://example.com/path/to/resource");

    EXPECT_EQ(with_path.protocol, "https");
    EXPECT_EQ(with_path.host, "example.com");
    EXPECT_EQ(with_path.path, "/path/to/resource");
}

TEST(URL, query) {
    URL with_query("https://example.com/search?q=openai");

    EXPECT_EQ(with_query.protocol, "https");
    EXPECT_EQ(with_query.host, "example.com");
    EXPECT_EQ(with_query.path, "/search");
    EXPECT_EQ(with_query.queries.at("q"), "openai");
}

TEST(URL, fragment) {
    URL with_fragment("ftp://example.com/index.html#section1");

    EXPECT_EQ(with_fragment.protocol, "ftp");
    EXPECT_EQ(with_fragment.host, "example.com");
    EXPECT_EQ(with_fragment.path, "/index.html");
    EXPECT_EQ(with_fragment.fragment, "section1");
}

TEST(URL, malformed) {
    URL malformed_url("https:///example.com");

    EXPECT_EQ(malformed_url.protocol, "https");
    EXPECT_EQ(malformed_url.host, "");
    EXPECT_EQ(malformed_url.path, "/example.com");
}

TEST(URL, ipv4) {
    URL with_ipv4("http://192.168.1.1/admin");

    EXPECT_EQ(with_ipv4.protocol, "http");
    EXPECT_EQ(with_ipv4.host, "192.168.1.1");
    EXPECT_EQ(with_ipv4.path, "/admin");
}

TEST(URL, ipv6) {
    URL with_ipv6("http://[2001:db8::1]:8080/admin");

    EXPECT_EQ(with_ipv6.url, "http://[2001:db8::1]:8080/admin");
    EXPECT_EQ(with_ipv6.protocol, "http");
    EXPECT_EQ(with_ipv6.host, "[2001:db8::1]:8080");
    EXPECT_EQ(with_ipv6.path, "/admin");
}



