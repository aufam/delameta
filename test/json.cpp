#include <boost/preprocessor.hpp> // to enable JSON_DECLARE macro
#include <delameta/json.h>
#include <gtest/gtest.h>

using namespace Project;
namespace json = delameta::json;
using delameta::Stream;

static void test_object(auto o, int expected_stream_rules_count) {
    auto j = json::serialize(o);                // convert to json string
    EXPECT_TRUE(etl::Json::parse(j.c_str()));   // validate json string
    EXPECT_EQ(json::size_max(o), j.size());     // validate predicted json string length
    
    Stream s = json::serialize_as_stream(std::move(o)); // convert to stream
    std::string out;
    int stream_rules_count = 0;
    s >> [&](std::string_view sv) {
        out += sv;
        ++stream_rules_count;
    };

    EXPECT_EQ(out, j);  // validate json stream
    EXPECT_EQ(stream_rules_count, expected_stream_rules_count); // validate rules count
}

JSON_DECLARE(
    (Person),
    (std::string, name)
    (int, age)
)

TEST(Json, struct) {
    int expected_stream_rules_count = 2; // 1 open curly bracket + 2 struct items + 1 close curly bracket
    test_object(Person{.name="Sugeng", .age=42}, expected_stream_rules_count);

    Person o{.name="Madun", .age=21};
    test_object(etl::ref_const(o), expected_stream_rules_count);
}

TEST(Json, map) {
    test_object(json::Map {
        {"name", std::string("Jupri")},
        {"age", 19},
        {"is_married", true},
        {"salary", 9.1},
        {"role", nullptr},
    }, 5);

    test_object(std::unordered_map<std::string_view, std::string> {
        {"name", "Sapri"},
        {"role", "Marketing"},
        {"age", "12"},
    }, 3);

    test_object(etl::UnorderedMap<std::string_view, std::string> {
        {"name", "Bowo"},
        {"role", "Engineer"},
        {"age", "42"},
    }, 3);
}

TEST(Json, list) {
    test_object(json::List {
        42,
        3.14,
        std::string("123"),
        false,
        nullptr,
    }, 5);

    test_object(std::vector<std::string_view> {
        "42",
        "123",
        "false",
        "text",
    }, 4);

    test_object(std::list<int> {
        42,
        72,
        13,
    }, 3);

    test_object(std::vector<std::optional<std::string>> {
        "text",
        std::nullopt,
    }, 2);

    test_object(etl::LinkedList<bool> {
        true,
        false,
    }, 2);
}