#include "delameta/url.h"
#include "delameta/error.h"
#include "delameta/debug.h"
#include <etl/string_view.h>
#include <algorithm>

using namespace Project;
using namespace Project::delameta;

using etl::Err;
using etl::Ok;

struct addrinfo {
    uint8_t ip[4];
    int port;
};

static const std::pair<const char*, int> protocol_default_ports[] = {
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

auto delameta_detail_resolve_domain(const std::string& domain) -> Result<addrinfo> {
    URL url = domain;
    auto sv = etl::string_view(url.host.data(), url.host.size());
    auto [host_sv, port_sv] = parse_host(sv);
    int port = 0;
    std::string host(host_sv.data(), host_sv.len());

    if (port_sv.len() > 0) {
        port = port_sv.to_int();
    } else {
        auto it = std::find_if(std::begin(protocol_default_ports), std::end(protocol_default_ports), 
        [&](std::pair<const char*, int> item) {
            return item.first == url.protocol;
        });
        port = it == std::end(protocol_default_ports) ? 80 : it->second;
    }

    addrinfo ip;
    ip.port = port;

    auto ip_ = host_sv.split<4>(".");
    if (ip_.len() != 4) {
        return Err(Error{-1, "invalid ip"});
    }
    ip.ip[0] = ip_[0].to_int();
    ip.ip[1] = ip_[1].to_int();
    ip.ip[2] = ip_[2].to_int();
    ip.ip[3] = ip_[3].to_int();

    return Ok(ip);
}
