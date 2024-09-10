#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <limits.h>
#include <etl/string_view.h>
#include "delameta/stream.h"
#include "delameta/url.h"
#include "helper.h"

namespace Project::delameta {
    __attribute__((weak)) 
    void info(const char*, int, const std::string&) {}

    __attribute__((weak)) 
    void warning(const char*, int, const std::string&) {}

    __attribute__((weak)) 
    void panic(const char* file, int line, const std::string& msg) { 
        std::cerr << file << ":" << std::to_string(line) << ": " << msg << '\n';
        exit(1); 
    }
}

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;


int delameta_detail_set_non_blocking(int socket) {
    return ::fcntl(socket, F_SETFL, ::fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
}

bool delameta_detail_is_fd_alive(int fd) {
    return ::fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

bool delameta_detail_is_socket_alive(int socket) {
    uint8_t dummy;
    auto res = ::recv(socket, &dummy, 1, MSG_PEEK);
    if (res == 0)
        return false; // connection close
    if (res > 0)
        return true; // data available, socket is alive
    if (res < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
        return true; // no data available, socket is alive
    return false;
}

auto delameta_detail_get_ip(int socket) -> std::string {
    sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(socket, (sockaddr*)&addr, &addr_len) != 0) {
        warning(__FILE__, __LINE__, ::strerror(errno));
        return "<Invalid socket>";
    }

    char ip_str[INET6_ADDRSTRLEN];
    void* addr_ptr = nullptr;

    if (addr.ss_family == AF_INET) { // IPv4
        addr_ptr = &((sockaddr_in*)&addr)->sin_addr;
    } else if (addr.ss_family == AF_INET6) { // IPv6
        addr_ptr = &((sockaddr_in6*)&addr)->sin6_addr;
    } else {
        warning(__FILE__, __LINE__, "Unknown address family");
        return "<Invalid socket>";
    }

    if (inet_ntop(addr.ss_family, addr_ptr, ip_str, sizeof(ip_str)) == nullptr) {
        warning(__FILE__, __LINE__, ::strerror(errno));
        return "<Invalid socket>";
    }

    return std::string(ip_str);
}

auto delameta_detail_get_filename(int fd) -> std::string {
    char filename[PATH_MAX];
    ssize_t len = readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), filename, sizeof(filename) - 1);

    if (len != -1) {
        filename[len] = '\0';
        return std::string(filename);
    } else {
        warning(__FILE__, __LINE__, ::strerror(errno));
        return "<Invalid fd>";
    }
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
    warning(file, line, "FD " + std::to_string(fd) + ": Error: " + err.what);
    return Err(err);
}

static auto log_received_ok(const char* file, size_t line, int fd, std::vector<uint8_t>& res) {
    info(file, line, "FD " + std::to_string(fd) + " read " + std::to_string(res.size()) + " bytes");
    return Ok(std::move(res));
}

static auto log_sent_ok(const char* file, size_t line, int fd, size_t n) {
    info(file, line, "FD " + std::to_string(fd) + " write " + std::to_string(n) + " bytes");
    return Ok();
}

auto delameta_detail_read(const char* file, int line, int fd, void* ssl, int timeout, bool(*is_alive)(int)) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    int bytes_available = 0;
    auto ssl_ = reinterpret_cast<SSL*>(ssl);

    while (is_alive(fd)) {
        if (::ioctl(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        std::vector<uint8_t> buffer(bytes_available);
        auto size = ssl ? SSL_read(ssl_, buffer.data(), bytes_available) : ::read(fd, buffer.data(), bytes_available);
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
    int bytes_available = 0;

    while (delameta_detail_is_socket_alive(fd)) {
        if (::ioctl(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        std::vector<uint8_t> buffer(bytes_available);
        auto peer_ = reinterpret_cast<struct addrinfo *>(peer);
        socklen_t len_ = peer_->ai_addrlen;
        auto size = ::recvfrom(fd, buffer.data(), bytes_available, 0, peer_->ai_addr, &len_);
        if (size < 0) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        buffer.resize(size);
        return log_received_ok(file, line, fd, buffer);
    }

    return log_err(file, line, fd, Error::ConnectionClosed);
}

auto delameta_detail_read_until(const char* file, int line, int fd, void* ssl, int timeout, bool(*is_alive)(int), size_t n) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> buffer(n);

    int remaining_size = n;
    int bytes_available = 0;
    auto ptr = buffer.data();
    auto ssl_ = reinterpret_cast<SSL*>(ssl);

    while (is_alive(fd)) {
        if (::ioctl(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto size = ssl ? SSL_read(ssl_, ptr, std::min(bytes_available, remaining_size)) : ::read(fd, ptr, std::min(bytes_available, remaining_size));
        if (size < 0) {
            if (ssl) {
                char buf[128];
                auto code = ERR_get_error();
                ERR_error_string_n(code, buf, 128);
                panic(__FILE__, __LINE__, buf);
                return Err(Error{int(code), buf});
            }
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
    int bytes_available = 0;
    auto ptr = buffer.data();

    while (delameta_detail_is_socket_alive(fd)) {
        if (::ioctl(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(file, line, fd, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto peer_ = reinterpret_cast<struct addrinfo *>(peer);
        socklen_t len_ = peer_->ai_addrlen;
        auto size = ::recvfrom(fd, buffer.data(), bytes_available, 0, peer_->ai_addr, &len_);
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

auto delameta_detail_read_as_stream(const char* file, int line, int timeout, Descriptor* self, size_t n) -> Stream {
    Stream s;

    s << [self, file, line, total=n, buffer=std::vector<uint8_t>{}](Stream& s) mutable -> std::string_view {
        size_t n = std::min(total, (size_t)MAX_HANDLE_SZ);
        auto data = self->read_until(n);

        if (data.is_ok()) {
            buffer = std::move(data.unwrap());
            total -= n;
            s.again = total > 0;
        } else {
            buffer = {};
            warning(file, line, data.unwrap_err().what);
        }
        
        return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
    };

    return s;
}

auto delameta_detail_write(const char* file, int line, int fd, void* ssl, int timeout, bool(*is_alive)(int), std::string_view data) -> Result<void> {
    auto ssl_ = reinterpret_cast<SSL*>(ssl);
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        if (!is_alive(fd)) {
            return log_err(file, line, fd, Error::ConnectionClosed);
        }

        auto n = std::min<size_t>(MAX_HANDLE_SZ, data.size() - i);
        auto sent = ssl ? SSL_write(ssl_, &data[i], n) : ::write(fd, &data[i], n);
        
        if (sent == 0) {
            return log_err(file, line, fd, Error::ConnectionClosed);
        } else if (sent < 0) {
            if (ssl) {
                char buf[128];
                auto code = ERR_get_error();
                ERR_error_string_n(code, buf, 128);
                panic(__FILE__, __LINE__, buf);
                return Err(Error{int(code), buf});
            }
            int code = errno;
            std::string what = ::strerror(code);
            warning(file, line, what);
            return Err(Error{code, std::move(what)});
        }

        total += sent;
        i += sent;
    }

    return log_sent_ok(file, line, fd, total);
}

auto delameta_detail_sendto(const char* file, int line, int fd, int timeout, void* peer, std::string_view data) -> Result<void> {
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
            int code = errno;
            std::string what = ::strerror(code);
            warning(file, line, what);
            return Err(Error{code, std::move(what)});
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