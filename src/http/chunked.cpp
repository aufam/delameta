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

    using bytes = std::vector<uint8_t>;
    struct Chunked { std::string_view len, data; };

    static const auto read_one_chunk = [](Descriptor &input, bytes &buffer, size_t &residu) -> std::string_view {
        if (residu > 0) {
            buffer.erase(buffer.begin(), buffer.begin() + residu);
        }

        auto sv = delameta::string_view_from(buffer);
        size_t len_pos = sv.find("\r\n");

        while (true) {
            if (len_pos == std::string::npos) {
                auto read_result = input.read();
                if (read_result.is_err()) {
                    return "";
                }

                auto &read_data = read_result.unwrap();
                buffer.insert(buffer.end(), read_data.begin(), read_data.end());

                sv = delameta::string_view_from(buffer);
                len_pos = sv.find("\r\n");
                continue;
            }

            auto len_res = delameta::string_hex_into<size_t>(sv.substr(0, len_pos));
            if (len_res.is_err()) {
                return "";
            }

            auto len = len_res.unwrap();
            auto total_read = len_pos + 2 + len + 2;

            if (sv.size() < total_read) {
                len_pos = std::string::npos; // read again
                continue;
            }

            residu = total_read;
            return sv.substr(len_pos + 2, len);
        }
    };

    s << [&input, buffer=std::vector<uint8_t>(), residu=size_t(0)](Stream& s) mutable -> std::string_view {
        auto res = read_one_chunk(input, buffer, residu);
        s.again = res != "";
        return res;
    };

    return s;
}
