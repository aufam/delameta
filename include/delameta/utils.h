#ifndef PROJECT_DELAMETA_UTILS_H
#define PROJECT_DELAMETA_UTILS_H

#include <string_view>
#include <etl/result.h>

namespace Project::delameta {
    inline constexpr int char_bin_into_int(char ch) {
        return ch == '0' ? 0 : ch == '1' ? 1 : -1;
    }

    inline constexpr int char_dec_into_int(char ch) {
        return (ch >= '0' && ch <= '9') ? ch - '0' : -1;
    }

    inline constexpr int char_hex_into_int(char ch) {
        return (ch >= '0' && ch <= '9') ? ch - '0' :
            (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 :
            (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : -1;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    inline constexpr auto string_dec_into(std::string_view sv) -> etl::Result<uint16_t, const char*> {
        T res = 0;
        int dec_shift = 1;

        for (size_t i = sv.length(); i > 0; --i) {
            int num = char_dec_into_int(sv[i - 1]);
            if (num < 0)
                return etl::Err("invalid dec string");

            res += num * dec_shift;
            dec_shift *= 10;
        }

        return etl::Ok(res);
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    inline constexpr auto string_hex_into(std::string_view sv) -> etl::Result<T, const char*> {
        T res = 0;
        int bit_shift = 0;

        for (size_t i = sv.length(); i > 0; --i) {
            int num = char_hex_into_int(sv[i - 1]);
            if (num < 0)
                return etl::Err("invalid dec string");

            res |= static_cast<T>(num << bit_shift);
            bit_shift += 4;
        }

        return etl::Ok(res);
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    inline constexpr auto string_bin_into(std::string_view sv) -> etl::Result<T, const char*> {
        T res = 0;
        int bit_shift = 0;

        for (size_t i = sv.length(); i > 0; --i) {
            int num = char_bin_into_int(sv[i - 1]);
            if (num < 0)
                return etl::Err("invalid bin string");

            res |= static_cast<T>(num << bit_shift);
            bit_shift++;
        }

        return etl::Ok(res);
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    inline constexpr auto string_num_into(std::string_view sv) -> etl::Result<T, const char*> {
        if (sv.substr(0, 2) == "0x") {
            return string_hex_into<T>(sv.substr(2));
        } else if (sv.substr(0, 2) == "0b") {
            return string_bin_into<T>(sv.substr(2));
        } else {
            return string_dec_into<T>(sv);
        }
    }
}



#endif