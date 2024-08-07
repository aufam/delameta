#include "delameta/tcp/client.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstring>
#include "../delameta.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

auto delameta_detail_resolve_domain(const std::string& domain, bool for_binding) -> etl::Result<struct addrinfo*, int>;

static std::string debug_sockaddr(const struct sockaddr* sa) {
    char ipStr[INET6_ADDRSTRLEN]; // Enough space for both IPv4 and IPv6 addresses

    switch (sa->sa_family) {
        case AF_INET: { // IPv4
            struct sockaddr_in* sa_in = (struct sockaddr_in*)sa;
            inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, sizeof(ipStr));
            return std::string("IPv4 Address: ") + ipStr + ", Port: " + std::to_string(ntohs(sa_in->sin_port));
        }
        case AF_INET6: { // IPv6
            struct sockaddr_in6* sa_in6 = (struct sockaddr_in6*)sa;
            inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ipStr, sizeof(ipStr));
            return std::string("IPv6 Address: ") + ipStr + ", Port: " + std::to_string(ntohs(sa_in6->sin6_port));
            break;
        }
        default:
            return std::string("Unknown AF: ") + std::to_string(sa->sa_family);
    }
}

auto tcp::Client::New(const char* file, int line, Args args) -> Result<Client> {
    auto log_error = [file, line](int code, auto err_to_str) {
        std::string what = err_to_str(code);
        warning(file, line, what);
        return Error{code, what};
    };

    auto [resolve, resolve_err] = delameta_detail_resolve_domain(args.host, false);
    if (resolve_err) return Err(log_error(*resolve_err, ::gai_strerror));

    auto hint = *resolve;
    auto se = defer | [&]() { ::freeaddrinfo(hint); };
    Error err{-1, "Unable to resolve hostname: " + args.host};

    for (auto p = hint; p != nullptr; p = p->ai_next) {
        auto [client, client_err] = Socket::New(file, line, p->ai_family, p->ai_socktype, p->ai_protocol);
        if (client_err) {
            err = std::move(*client_err);
            continue;
        }
        
        if (::connect(client->socket, p->ai_addr, p->ai_addrlen) != 0) {
            if (errno != EINPROGRESS) {
                err = log_error(errno, ::strerror);
                continue;
            }

            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(client->socket, &write_fds);

            struct timeval tv;
            tv.tv_sec = args.timeout;
            tv.tv_usec = 0;

            if (::select(client->socket + 1, nullptr, &write_fds, nullptr, &tv) <= 0) {
                err = log_error(errno, ::strerror);
                continue;
            }

            // Connection established or error occurred
            int error_code = 0;
            socklen_t len = sizeof(error_code);
            if (::getsockopt(client->socket, SOL_SOCKET, SO_ERROR, &error_code, &len) != 0 || error_code != 0){
                err = log_error(errno, ::strerror);
                continue;
            }
        } 

        info(file, line, "Created socket client: " + std::to_string(client->socket));
        return Ok(Client(new Socket(std::move(*client))));
    }

    return Err(std::move(err));
}

tcp::Client::Client(Socket* socket) : socket(socket) {}

tcp::Client::Client(Client&& other) : socket(std::exchange(other.socket, nullptr)) {}

auto tcp::Client::operator=(Client&& other) -> Client& {
    if (this == &other) {
        return *this;
    }

    this->~Client();
    socket = std::exchange(other.socket, nullptr);
    return *this;
}

tcp::Client::~Client() {
    if (socket) {
        delete socket;
        socket = nullptr;
    }
}

auto tcp::Client::request(Stream& in_stream) -> Result<std::vector<uint8_t>> {
    if (socket == nullptr) {
        std::string what = "No client socket created";
        warning(__FILE__, __LINE__, what);
        return Err(Error{-1, what});
    }

    in_stream >> *socket;
    return socket->read();
}
