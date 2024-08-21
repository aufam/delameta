#include <boost/preprocessor.hpp>
#include <fmt/format.h>
#include <delameta/debug.h>
#include <delameta/error.h>
#include <delameta/json.h>
#include <delameta/http/server.h>
#include <gtest/gtest.h>

using namespace Project;

struct Foo {
    int num;
    std::string text;
};

JSON_TRAITS(
    (Foo),
    (int        , num)
    (std::string, text)
)

FMT_TRAITS(
    (Foo),
    (int        , num)
    (std::string, text)
)

TEST(Macro, traits) {
    Foo foo {
        .num = 42,
        .text = "text"
    };

    EXPECT_EQ(fmt::format("{}", foo), "Foo { num: 42, text: text, }");
    EXPECT_EQ(etl::json::serialize(foo), "{\"num\":42,\"text\":\"text\"}");

    std::string s;
    etl::detail::json_append(s, std::string("num"), foo.num);
    EXPECT_EQ(s, "\"num\":42,");

    auto deser_foo = etl::json::deserialize<Foo>("{\"num\":42,\"text\":\"text\"}").unwrap();
    EXPECT_EQ(deser_foo.num, 42);
    EXPECT_EQ(deser_foo.text, "text");
}

TEST(Macro, error) {
    delameta::Error e(-1, "Fatal error");
    EXPECT_EQ(fmt::format("{}", e), "Error {code: -1, what: Fatal error}");

    delameta::http::Server::Error he(delameta::http::StatusOK, "OK");
    EXPECT_EQ(fmt::format("{}", he), "http::Error {code: 200, what: OK}");
}

// it seems that you dont understand the result API, here is the tested test case
TEST(Macro, try) {
    static auto foo = []() -> delameta::Result<int> {
        return etl::Ok(42);
    };
    static auto bar = []() -> delameta::Result<int> {
        return etl::Err(delameta::Error(-1, "Fatal error"));
    };

    int first_num = 0;

    static auto baz = [&first_num]() -> delameta::Result<int> {
        int num = TRY(foo()); // ok, proceed to next line
        first_num = num; // first_num will be 42
        num += TRY(bar()); // fail, return the error
        return etl::Ok(num); // will never get here
    };

    auto res = baz();

    EXPECT_FALSE(res.is_ok());
    EXPECT_EQ(first_num, 42);
}