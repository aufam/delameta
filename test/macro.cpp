#include <boost/preprocessor.hpp>
#include <fmt/format.h>
#include <delameta/debug.h>
#include <delameta/error.h>
#include <delameta/json.h>
#include <delameta/http/http.h>
#include <gtest/gtest.h>

using namespace Project;
namespace json = delameta::json;
namespace http = delameta::http;
using delameta::Error;
using delameta::Result;
using etl::Ok;
using etl::Err;

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

    EXPECT_EQ(fmt::to_string(foo), "Foo { num: 42, text: text, }");
    EXPECT_EQ(json::serialize(foo), "{\"num\":42,\"text\":\"text\"}");

    std::string s;
    etl::detail::json_append(s, std::string("num"), foo.num);
    EXPECT_EQ(s, "\"num\":42,");

    auto deser_foo = json::deserialize<Foo>("{\"num\":42,\"text\":\"text\"}").unwrap();
    EXPECT_EQ(deser_foo.num, 42);
    EXPECT_EQ(deser_foo.text, "text");
}

TEST(Macro, error) {
    Error e(-1, "Fatal error");
    EXPECT_EQ(fmt::to_string(e), "Error {code: -1, what: Fatal error}");

    http::Error he(http::StatusOK, "OK");
    EXPECT_EQ(fmt::to_string(he), "http::Error {code: 200, what: OK}");
}

TEST(Macro, try) {
    auto foo = []() -> Result<int> {
        return Ok(42);
    };
    auto bar = []() -> Result<int> {
        return Err(Error(-1, "Fatal error"));
    };

    int num_test = 0;

    auto baz = [&]() -> Result<int> {
        int num = TRY(foo()); // ok, proceed to next line
        num_test = num; // num_test will be 42
        num_test += TRY(bar()); // fail, return the error
        return Ok(num); // will never get here
    };

    auto res = baz();

    EXPECT_EQ(num_test, 42);
    EXPECT_FALSE(res.is_ok());
    EXPECT_TRUE(res.is_err());

    EXPECT_THROW(res.unwrap(), std::bad_variant_access);
    EXPECT_EQ(res.unwrap_err().code, -1);
    EXPECT_EQ(res.unwrap_err().what, "Fatal error");
}