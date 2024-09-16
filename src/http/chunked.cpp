#include "delameta/http/chunked.h"
#include "delameta/utils.h"

using namespace Project;
using namespace delameta;

Stream http::chunked_encode(Stream& input) {
    Stream s;

    s << [input=new Stream(std::move(input)), buffer=std::string()](Stream& s) mutable -> std::string_view {
        auto data = input->pop_once();
        buffer.clear();
        buffer.reserve(6 + data.size() + 2);

        buffer += num_to_hex_string(data.size()) + "\r\n";
        buffer.insert(buffer.end(), data.begin(), data.end());
        buffer += "\r\n";

        s.again = !data.empty();

        if (!s.again) {
            delete input;
        }

        return buffer;
    };

    return s;
}

Stream http::chunked_decode(Descriptor& input) {
    Stream s;

    s << [&input, buffer=std::string()](Stream& s) mutable -> std::string_view {
        buffer.clear();
        for (;;) {
            auto read_result = input.read_until(1);
            if (read_result.is_err())
                return "";

            char ch = (char)read_result.unwrap()[0];
            if (ch == '\n') 
                break;

            buffer += ch;
        }

        if (buffer.back() == '\r') buffer.pop_back(); 
        auto size = string_hex_into<int>(buffer);
        if (size.is_err())
            return "";

        auto n = size.unwrap();
        buffer.clear();

        if (n > 0) {
            buffer.reserve(n);

            auto read_result = input.read_until(n);
            if (read_result.is_err())
                return "";

            buffer.insert(buffer.end(), read_result.unwrap().begin(), read_result.unwrap().end());

            for (;;) {
                auto read_result = input.read_until(1);
                if (read_result.is_err())
                    return "";

                char ch = (char)read_result.unwrap()[0];
                if (ch == '\n') 
                    break;
            }
        } 
        s.again = !buffer.empty();

        return buffer;
    };

    return s;
}
