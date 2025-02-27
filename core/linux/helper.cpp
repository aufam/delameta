#include <cstring>
#include <fmt/format.h>
#include <iostream>
#include <thread>
#include <etl/string_view.h>
#include <algorithm>
#include "delameta/stream.h"
#include "delameta/url.h"
#include "helper.h"

// Unix/Linux headers and definitions
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#define IOCTL ::ioctl

#ifndef DELAMETA_DISABLE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

namespace Project::delameta {
    void __attribute__((weak)) info(const char*, int, const std::string&) {}

    void __attribute__((weak)) warning(const char*, int, const std::string&) {}

    void __attribute__((weak)) panic(const char* file, int line, const std::string& msg) {
        std::cerr << fmt::format("{}:{} {}\n", file, line, msg);
        exit(1);
    }
}

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;

int delameta_detail_set_non_blocking(int socket) {
    return ::fcntl(socket, F_SETFL, ::fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
}
int delameta_detail_set_blocking(int socket) {
    return ::fcntl(socket, F_SETFL, ::fcntl(socket, F_GETFL, 0) & ~O_NONBLOCK);
}
bool delameta_detail_is_fd_alive(int fd) {
    return ::fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

bool delameta_detail_is_socket_alive(int socket) {
    char dummy;
    int res = ::recv(socket, &dummy, 1, MSG_PEEK);
    if (res == 0)
        return false; // connection close
    if (res > 0)
        return true; // data available, socket is alive
    if (auto errno_ = errno; res < 0 && (errno_ == EWOULDBLOCK || errno_ == EINPROGRESS))
        return true; // no data available, socket is alive
    return false;
}

auto delameta_detail_get_ip(int socket) -> std::string {
    sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (::getpeername(socket, (sockaddr*)&addr, &addr_len) != 0) {
        PANIC(
            fmt::format("Cannot resolve IP address of socket {}: {}", socket, ::strerror(errno))
        );
    }

    char ip_str[INET6_ADDRSTRLEN];
    void* addr_ptr = nullptr;

    if (addr.ss_family == AF_INET) { // IPv4
        addr_ptr = &((sockaddr_in*)&addr)->sin_addr;
    } else if (addr.ss_family == AF_INET6) { // IPv6
        addr_ptr = &((sockaddr_in6*)&addr)->sin6_addr;
    } else {
        PANIC(
            fmt::format("Unknown address family. expect AF_INET(={}) or AF_INET6(={}) got {}",
                AF_INET, AF_INET6, addr.ss_family
            )
        );
    }

    if (::inet_ntop(addr.ss_family, addr_ptr, ip_str, sizeof(ip_str)) == nullptr) {
        PANIC(
            fmt::format("Cannot convert struct IP into string IP: {}", ::strerror(errno))
        );
    }

    return std::string(ip_str);
}

auto delameta_detail_get_filename(int fd) -> std::string {
    // Linux-specific implementation
    char filename[PATH_MAX];
    ssize_t len = ::readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), filename, sizeof(filename) - 1);

    if (len != -1) {
        filename[len] = '\0';
        return std::string(filename);
    } else {
        PANIC(fmt::format("Cannot resolve filename from FD {}: {}", fd, ::strerror(errno)));
        return "<Invalid fd>";
    }
}

auto delameta_detail_log_format_fd(int fd, const std::string& msg) -> std::string {
    return fmt::format("FD {}: {}", fd, msg);
}

static const std::unordered_map<std::string, int> protocol_default_ports = {
    {"http", 80},
    {"https", 443},
    {"ftp", 21},
    {"smtp", 25},
    {"pop3", 110},
    {"imap", 143},
    // TODO: Add more protocols and their default ports as needed
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

auto delameta_detail_resolve_domain(const std::string& domain, int sock_type, bool for_binding) -> etl::Result<struct addrinfo*, int> {
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
    hints.ai_socktype = sock_type;
    if (for_binding) hints.ai_flags = AI_PASSIVE; // For wildcard IP address

    if (int code = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &hint); code == 0) {
        return Ok(hint);
    } else {
        return Err(code);
    }
}

static auto log_err(const char* file, int line, int fd, Error err) {
    warning(file, line, delameta_detail_log_format_fd(fd, err.what));
    return Err(std::move(err));
}

static auto log_received_ok(const char* file, size_t line, int fd, std::vector<uint8_t>& res) {
    info(file, line, delameta_detail_log_format_fd(fd, "read " + std::to_string(res.size()) + " bytes"));
    return Ok(std::move(res));
}

static auto log_sent_ok(const char* file, size_t line, int fd, size_t n) {
    info(file, line, delameta_detail_log_format_fd(fd, "written " + std::to_string(n) + " bytes"));
    return Ok();
}

[[maybe_unused]] static auto log_err(const char* file, int line, Error err) {
    warning(file, line, err.what);
    return Err(std::move(err));
}

[[maybe_unused]] static auto log_received_ok(const char* file, size_t line, std::vector<uint8_t>& res) {
    info(file, line, "read " + std::to_string(res.size()) + " bytes");
    return Ok(std::move(res));
}

[[maybe_unused]] static auto log_sent_ok(const char* file, size_t line, size_t n) {
    info(file, line, "written " + std::to_string(n) + " bytes");
    return Ok();
}

auto delameta_detail_read(const char* file, int line, int fd, [[maybe_unused]] void* ssl, int timeout, bool(*is_alive)(int)) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    unsigned long bytes_available = 0;

#ifndef DELAMETA_DISABLE_OPENSSL
    auto ssl_ = reinterpret_cast<SSL*>(ssl);
#endif

    while (is_alive(fd)) {
        if (IOCTL(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout >= 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        std::vector<uint8_t> buffer(bytes_available);

#ifndef DELAMETA_DISABLE_OPENSSL
        auto size = ssl ? SSL_read(ssl_, buffer.data(), bytes_available) : ::read(fd, buffer.data(), bytes_available);
#else
        auto size = ::read(fd, (char*)buffer.data(), bytes_available);
#endif
        if (size < 0) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        buffer.resize(size);
        return log_received_ok(file, line, fd, buffer);
    }

    return log_err(file, line, fd, Error::ConnectionClosed);
}

auto delameta_detail_recvfrom(const char* file, int line, int fd, int timeout, void *peer) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    unsigned long bytes_available = 0;

    while (delameta_detail_is_socket_alive(fd)) {
        if (IOCTL(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout >= 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        std::vector<uint8_t> buffer(bytes_available);
        auto peer_ = reinterpret_cast<struct addrinfo *>(peer);
        socklen_t len_ = peer_->ai_addrlen;
        auto size = ::recvfrom(fd, (char*)buffer.data(), bytes_available, 0, peer_->ai_addr, &len_);
        if (size < 0) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        buffer.resize(size);
        return log_received_ok(file, line, fd, buffer);
    }

    return log_err(file, line, fd, Error::ConnectionClosed);
}

auto delameta_detail_read_until(const char* file, int line, int fd, [[maybe_unused]] void* ssl, int timeout, bool(*is_alive)(int), size_t n) -> Result<std::vector<uint8_t>> {
#ifndef DELAMETA_DISABLE_OPENSSL
    if (ssl) { // cannot read parsial data
        return delameta_detail_read(file, line, fd, ssl, timeout, is_alive);
    }
#endif

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> buffer(n);

    int remaining_size = n;
    unsigned long bytes_available = 0;
    auto ptr = buffer.data();

    while (is_alive(fd)) {
        if (IOCTL(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout >= 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto size = ::read(fd, ptr, std::min((int)bytes_available, remaining_size));
        if (size < 0) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        ptr += size;
        remaining_size -= size;

        if (remaining_size <= 0) {
            return log_received_ok(file, line, fd, buffer);
        }
    }

    return log_err(file, line, fd, Error::ConnectionClosed);
}

auto delameta_detail_recvfrom_until(const char* file, int line, int fd, int timeout, void *peer, size_t n) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> buffer(n);

    int remaining_size = n;
    unsigned long bytes_available = 0;

    while (delameta_detail_is_socket_alive(fd)) {
        if (IOCTL(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout >= 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto peer_ = reinterpret_cast<struct addrinfo *>(peer);
        socklen_t len_ = peer_->ai_addrlen;
        auto size = ::recvfrom(fd, (char*)buffer.data(), bytes_available, 0, peer_->ai_addr, &len_);
        if (size < 0) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        remaining_size -= size;

        if (remaining_size <= 0) {
            return log_received_ok(file, line, fd, buffer);
        }
    }

    return log_err(file, line, fd, Error::ConnectionClosed);
}

auto delameta_detail_read_as_stream(const char*, int, int timeout, Descriptor* self, size_t n) -> Stream {
    (void)timeout;
    Stream s;

    s << [self, total=n, buffer=std::vector<uint8_t>{}](Stream& s) mutable -> std::string_view {
        size_t n = std::min(total, (size_t)MAX_HANDLE_SZ);
        auto data = self->read_until(n);

        if (data.is_ok()) {
            buffer = std::move(data.unwrap());
            n = buffer.size();
            total = total > n ? total - n : 0;
            s.again = total > 0;
        } else {
            buffer = {};
        }

        return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
    };

    return s;
}

auto delameta_detail_write(const char* file, int line, int fd, [[maybe_unused]] void* ssl, int timeout, bool(*is_alive)(int), std::string_view data) -> Result<void> {
    (void)timeout;
#ifndef DELAMETA_DISABLE_OPENSSL
    auto ssl_ = reinterpret_cast<SSL*>(ssl);
#endif
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        if (!is_alive(fd)) {
            return log_err(file, line, fd, Error::ConnectionClosed);
        }

        auto n = std::min<size_t>(MAX_HANDLE_SZ, data.size() - i);
#ifndef DELAMETA_DISABLE_OPENSSL
        auto sent = ssl ? SSL_write(ssl_, &data[i], n) : ::write(fd, &data[i], n);
#else
        auto sent = ::write(fd, &data[i], n);
#endif

        if (sent == 0) {
            return log_err(file, line, fd, Error::ConnectionClosed);
        } else if (sent < 0) {
#ifndef DELAMETA_DISABLE_OPENSSL
            if (ssl) {
                char buf[128];
                auto code = ERR_get_error();
                ERR_error_string_n(code, buf, 128);
                return log_err(file, line, fd, Error{int(code), buf});
            }
#endif
            auto errno_ = errno;
            if (errno_ == EWOULDBLOCK || errno_ == EINPROGRESS) {
                std::this_thread::sleep_for(10ms);
                continue; // maybe try again
            }
            return log_err(file, line, fd, Error(errno_, ::strerror(errno_)));
        }

        total += sent;
        i += sent;
    }

    return log_sent_ok(file, line, fd, total);
}

auto delameta_detail_sendto(const char* file, int line, int fd, int timeout, void* peer, std::string_view data) -> Result<void> {
    (void)timeout;
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        if (!delameta_detail_is_socket_alive(fd)) {
            return log_err(file, line, fd, Error::ConnectionClosed);
        }

        auto n = std::min<size_t>(MAX_HANDLE_SZ, data.size() - i);
        auto peer_ = reinterpret_cast<struct addrinfo *>(peer);
        auto sent = ::sendto(fd, &data[i], n, 0, peer_->ai_addr, peer_->ai_addrlen);

        if (sent == 0) {
            return log_err(file, line, fd, Error::ConnectionClosed);
        } else if (sent < 0) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        total += sent;
        i += sent;
    }

    return log_sent_ok(file, line, fd, total);
}

auto delameta_detail_create_socket(void* hint, const LogError& log_error) -> Result<int> {
    auto h = reinterpret_cast<struct addrinfo*>(hint);
    auto socket = ::socket(h->ai_family, h->ai_socktype, h->ai_protocol);
    if (socket < 0) {
        return Err(log_error(errno, ::strerror));
    }

    delameta_detail_set_non_blocking(socket);

    if (int enable = 1; ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        ::close(socket);
        return Err(log_error(errno, ::strerror));
    }

    return Ok(socket);
}

void delameta_detail_close_socket(int socket) {
    ::close(socket);
}

