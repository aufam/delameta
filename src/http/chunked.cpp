#include "delameta/http/chunked.h"
#include "delameta/utils.h"

using namespace Project;
using namespace delameta;

Stream http::chunked_encode(Stream& inp) {
    Stream s;

    auto input = new Stream(std::move(inp));
    s << [input, buffer=std::string()](Stream& s) mutable -> std::string_view {
        auto data = input->pop_once();
        buffer.clear();
        buffer.reserve(6 + data.size() + 2);

        buffer += num_to_hex_string(data.size()) + "\r\n";
        buffer.insert(buffer.end(), data.begin(), data.end());
        buffer += "\r\n";

        s.again = !data.empty();

        return buffer;
    };

    s.at_destructor = [input]() { delete input; };

    return s;
}

Stream http::chunked_decode(Descriptor& input) {
    Stream s;

    s << [
        &input, 
        buffer=std::vector<uint8_t>(), 
        sv=std::string_view(),
        do_append=true
    ](Stream& s) mutable -> std::string_view {
        while (true) {
            if (do_append) {
                auto read_result = input.read();
                if (read_result.is_err())
                    return "";

                auto &read_data = read_result.unwrap();
                if (sv.empty()) {
                    buffer = std::move(read_data);
                } else {
                    buffer.clear();
                    buffer.reserve(sv.size() + read_data.size());
                    buffer.insert(buffer.end(), sv.begin(), sv.end());
                    buffer.insert(buffer.end(), read_data.begin(), read_data.end());
                }
                sv = std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            }

            auto line = string_view_consume_line(sv);
            auto chunk_size_result = string_hex_into<size_t>(line);
            if (chunk_size_result.is_err()) {
                return "";
            }

            auto chunk_size = chunk_size_result.unwrap();
            if (chunk_size + 2 >= sv.size()) {
                auto res = sv.substr(0, chunk_size);
                sv = sv.substr(chunk_size + 2);
                do_append = sv.size() < 3;
                s.again = chunk_size > 0;
                return res;
            } else {
                do_append = true;
                s.again = true;
            }
        }
    };

    return s;
}
