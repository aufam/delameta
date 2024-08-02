#include "delameta/url.h"
#include <etl/string_view.h>
#include <netdb.h>
#include <arpa/inet.h>

using namespace Project;
using namespace Project::delameta;

static auto parse_query(etl::StringView sv) -> std::unordered_map<std::string, std::string>;
static auto sv_percent_encoded_to_string(etl::StringView sv) -> std::string;
static auto sv_to_string(etl::StringView sv) { return std::string(sv.data(), sv.len()); }
static auto resolve_domain(const std::string& protocol, const std::string& input) -> std::pair<std::string, int>;

static constexpr int hex_to_int(char ch) {
    return (ch >= '0' && ch <= '9') ? ch - '0' :
           (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 :
           (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : -1;
}

URL::URL(std::string s) : url(std::move(s)) {
    auto sv = etl::string_view(url.data(), url.size());

    if (sv.len() == 0)
        return;

    auto domain = sv.find("://");
    if (domain < sv.len()) {
        this->protocol = sv_to_string(sv.substr(0, domain));
        sv = sv.substr(domain + 3, sv.len() - (domain + 3));
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

    this->host = sv_to_string(sv.substr(dom.start, dom.stop - dom.start));
    if (!this->host.empty()) {
        auto [ip, port] = resolve_domain(this->protocol, this->host);
        this->ip = std::move(ip);
        this->port = port;
    }
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

static auto resolve_domain(const std::string& protocol, const std::string& input) -> std::pair<std::string, int> {
    auto sv = etl::string_view(input.data(), input.size());
    int port = 0;
    bool use_https = false;  // Default to HTTP if no protocol is specified
    auto [domain_sv, port_sv] = sv.split<2>(":");
    std::string domain(domain_sv.data(), domain_sv.end());

    if (port_sv.len() > 0) {
        port = port_sv.to_int();
    } else {
        auto it = protocol_default_ports.find(protocol);
        if (it != protocol_default_ports.end()) {
            port = it->second;
        } else {
            port = use_https ? 443 : 80;
        }
    }

    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Use AF_INET for IPv4
    hints.ai_socktype = SOCK_STREAM;
    if (::getaddrinfo(domain.c_str(), nullptr, &hints, &res) != 0) {
        return {std::move(domain), port}; // assuming the domain is already in ipv4 format
    }

    char ip_str[INET_ADDRSTRLEN];
    if (res != nullptr) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
        ::inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, sizeof(ip_str));
    }

    ::freeaddrinfo(res);

    return {ip_str, port};
}