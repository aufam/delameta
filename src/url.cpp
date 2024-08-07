#include "delameta/url.h"
#include <etl/string_view.h>
#include <etl/result.h>
#include <netdb.h>
#include <arpa/inet.h>

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

static const std::unordered_map<std::string, int> protocol_default_ports = {
    {"http", 80},
    {"https", 443},
    {"ftp", 21},
    {"smtp", 25},
    {"pop3", 110},
    {"imap", 143},
    // Add more protocols and their default ports as needed
};

static constexpr auto parse_host(etl::StringView domain) -> std::pair<etl::StringView, etl::StringView> {
    bool is_ipv6 = domain[0] == '[';
    if (!is_ipv6)  {
        auto pos = domain.find(":");
        if (pos >= domain.len()) {
            return {domain, ""};
        }
        return {domain.substr(0, pos), domain.substr(pos + 1, domain.len() - (pos + 1))};
    }

    auto pos = domain.find("]");
    if (pos >= domain.len()) {
        return {domain, ""};
    }

    auto host_sv = domain.substr(1, pos - 1);
    etl::StringView port_sv = "";
    if (domain[pos + 1] == ':') {
        port_sv = domain.substr(pos + 2, domain.len() - (pos + 2));
    }
    return {host_sv, port_sv};
}

auto delameta_detail_resolve_domain(const std::string& domain, bool for_binding) -> etl::Result<struct addrinfo*, int> {
    URL url = domain;
    auto sv = etl::string_view(url.host.data(), url.host.size());
    auto [host_sv, port_sv] = parse_host(sv);
    int port = 0;
    std::string host(host_sv.data(), host_sv.len());

    if (port_sv.len() > 0) {
        port = port_sv.to_int();
    } else {
        auto it = protocol_default_ports.find(url.protocol);
        port = it == protocol_default_ports.end() ? 80 : it->second;
    }

    struct addrinfo hints, *hint;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Allow for IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    if (for_binding) hints.ai_flags = AI_PASSIVE; // For wildcard IP address

    if (int code = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &hint); code == 0) {
        return etl::Ok(hint);
    } else {
        return etl::Err(code);
    }
}