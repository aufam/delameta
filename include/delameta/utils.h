#ifndef PROJECT_DELAMETA_UTILS_H
#define PROJECT_DELAMETA_UTILS_H

#include <string>
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

    inline std::string_view get_content_type_from_file(std::string_view file) {
        auto extension = file.substr(file.find_last_of('.') + 1);
        return 
            extension == "js"   ? "application/javascript" :
            extension == "json" ? "application/json" :
            extension == "pdf"  ? "application/pdf" :
            extension == "xml"  ? "application/xml" :
            extension == "css"  ? "text/css" :
            extension == "html" ? "text/html" :
            extension == "txt"  ? "text/plain" :
            extension == "jpeg" ? "image/jpeg" :
            extension == "jpg"  ? "image/jpeg" :
            extension == "png"  ? "image/png" :
            extension == "gif"  ? "image/gif" :
            extension == "mp4"  ? "video/mp4" :
            extension == "mpeg" ? "audio/mpeg" :
            extension == "mp3"  ? "audio/mpeg" :
            extension == "wav"  ? "audio/wav" :
            extension == "ogg"  ? "audio/ogg" :
            extension == "flac" ? "audio/flac" :
            extension == "avi"  ? "video/x-msvideo" :
            extension == "mov"  ? "video/quicktime" :
            extension == "webm" ? "video/webm" :
            extension == "mkv"  ? "video/x-matroska" :
            extension == "zip"  ? "application/zip" :
            extension == "rar"  ? "application/x-rar-compressed" :
            extension == "tar"  ? "application/x-tar" :
            extension == "gz"   ? "application/gzip" :
            extension == "doc"  ? "application/msword" :
            extension == "docx" ? "application/vnd.openxmlformats-officedocument.wordprocessingml.document" :
            extension == "ppt"  ? "application/vnd.ms-powerpoint" :
            extension == "pptx" ? "application/vnd.openxmlformats-officedocument.presentationml.presentation" :
            extension == "xls"  ? "application/vnd.ms-excel" :
            extension == "xlsx" ? "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" :
            extension == "odt"  ? "application/vnd.oasis.opendocument.text" :
            extension == "ods"  ? "application/vnd.oasis.opendocument.spreadsheet" :
            extension == "svg"  ? "image/svg+xml" :
            extension == "ico"  ? "image/x-icon" :
            extension == "md"   ? "text/markdown" :
            "application/octet-stream" // Default to binary data
        ;
    }

}



#endif