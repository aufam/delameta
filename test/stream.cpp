#include <delameta/stream.h>
#include <gtest/gtest.h>

using namespace Project;
using delameta::Stream;
using delameta::Descriptor;

TEST(Stream, rules) {
    static const std::string in_stream =
        "Some multiple lines\n"
        "Of data\n";
    
    Stream s;
    int pos = 0;
    s << [&pos, data=std::string()](Stream& self) mutable -> std::string_view {
        data = in_stream.substr(pos++, 1);
        self.again = data[0] != '\n';
        return data;
    };

    std::string out_stream;
    s >> [&](std::string_view sv) {
        out_stream += sv;
    };

    EXPECT_EQ(out_stream, "Some multiple lines\n");
}

TEST(Stream, move) {
    Stream a, b;
    a.at_destructor = [](){};
    b << a;

    EXPECT_FALSE(a.at_destructor);
    EXPECT_TRUE(b.at_destructor);
}
