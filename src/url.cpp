#include "delameta/url.h"
#include <etl/string_view.h>
#include <etl/result.h>

using namespace Project;
using namespace Project::delameta;
using etl::defer;

static auto parse_query(etl::StringView sv) -> std::unordered_map<std::string, std::string>;
static auto sv_percent_encoded_to_string(etl::StringView sv) -> std::string;
static auto sv_to_string(etl::StringView sv) { return std::string(sv.data(), sv.len()); }

static constexpr int hex_to_int(char ch) {
    return (ch >= '0' && ch <= '9') ? ch - '0' :
           (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 :
           (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : -1;
}

URL::URL(std::string s) : url(std::move(s)) {
    auto sv = etl::string_view(url.data(), url.size());
    if (sv.len() == 0)
        return;

    auto pos = sv.find("://");
    if (pos < sv.len()) {
        this->protocol = sv_to_string(sv.substr(0, pos));
        sv = sv.substr(pos + 3, sv.len() - (pos + 3));
    }
    
    struct { int start; int stop; bool found; } dom = {}, path = {}, que = {}, frag = {};
    int end = sv.len() - 1;

    for (int i = 0; i < end; ++i) {
        char ch = sv[i];

        if (not dom.found and ch == '/') {
            dom.start = 0;
            dom.stop = i;
            dom.found = true;
        }
        if (not path.found and ch == '/') {
            path.start = i;
            path.stop = sv.len();
            path.found = true;
        }
        if (not que.found and ch == '?') {
            que.start = i + 1;
            que.stop = sv.len();
            que.found = true;

            if (path.found) {
                path.stop = i;
            }
        } 
        if (not frag.found and ch == '#') {
            frag.start = i + 1;
            frag.stop = sv.len();

            // found '#' before '?'
            if (not que.found) {
                que = frag;
                path.stop = i;
            } 
            // found '#' after '?'
            else {
                que.stop = i;
            }
        } 
    }

    this->host = dom.found ? sv_to_string(sv.substr(dom.start, dom.stop - dom.start)) : sv_to_string(sv);

    if (path.found) {
        this->path = sv_to_string(sv.substr(path.start, path.stop - path.start));
        this->full_path = sv_to_string(sv.substr(path.start, sv.len() - path.start));
    } else {
        this->path = "/";
        this->full_path = "/";
    }
    this->queries = parse_query(sv.substr(que.start, que.stop - que.start));
    this->fragment = sv_to_string(sv.substr(frag.start, frag.stop - frag.start));
}

static auto sv_percent_encoded_to_string(etl::StringView sv) -> std::string {
    std::string res;
    res.reserve(sv.len() + 1);

    for (size_t i = 0; i < sv.len(); ++i) {
        if (sv[i] == '+') {
            res += ' ';
            continue;
        }
        if (sv[i] != '%') {
            res += sv[i];
            continue;
        }

        int a = hex_to_int(sv[i + 1]);
        int b = hex_to_int(sv[i + 2]);
        if (a >= 0 and b >= 0) {
            res += static_cast<char>(a << 4 | b);
            i += 2;
        }
    }

    return res;
}

static auto parse_query(etl::StringView sv) -> std::unordered_map<std::string, std::string> {
    auto res = std::unordered_map<std::string, std::string>();

    for (auto kv : sv.split<16>("&")) {
        auto [key, value] = kv.split<2>("=");
        res[sv_percent_encoded_to_string(key)] = sv_percent_encoded_to_string(value);
    }

    return res;
}

auto URL::decode(std::string_view sv) -> std::unordered_map<std::string, std::string> {
    return parse_query({sv.data(), sv.size()});
}

static constexpr bool is_unreserved_character(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c == '-') ||
           (c == '_') ||
           (c == '.') ||
           (c == '~');
}

static void char_to_hex(char c, std::string& out) {
    static const char digits[] = "0123456789ABCDEF";
    out += digits[(c >> 4) & 0xF];
    out += digits[c & 0xF];
}

auto URL::encode(const std::unordered_map<std::string, std::string>& queries) -> std::string {
    size_t total = 16;
    for (const auto& [k, v]: queries) {
        total += k.size() + 1 + v.size();
    }

    std::string res;
    res.reserve(total);
    bool first_item = true;

    for (const auto& [k, v]: queries) {
        if (first_item) {
            first_item = false;
        } else {
            res += '&';
        }

        for (char c: k) {
            if (is_unreserved_character(c)) {
                res += c;
            } else {
                res += '%';
                char_to_hex(c, res);
            }
        }

        res += '=';
        for (char c: v) {
            if (is_unreserved_character(c)) {
                res += c;
            } else {
                res += '%';
                char_to_hex(c, res);
            }
        }
    }

    return res;
}