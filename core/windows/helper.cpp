#include <fmt/format.h>
#include <iostream>
#include <thread>
#include <etl/string_view.h>
#include <algorithm>
#include "delameta/stream.h"
#include "delameta/url.h"
#include "helper.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <mutex>
#define IOCTL ::ioctlsocket
#define MAX_HANDLE_SZ 128
#undef min
static std::mutex windows_socket_mutex;
static int windows_socket_startup_counter;

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

static int get_bytes_available(int fd, bool is_wsa, u_long& bytes_available) {
    if (is_wsa) return IOCTL(fd, FIONREAD, &bytes_available);
    auto l = _filelength(fd);
    if (l >= 0) {
        bytes_available = (u_long)l;
        return 0;
    }
    return -1;
}

int delameta_detail_set_non_blocking(int socket) {
    u_long mode = 1; // 1 = non-blocking mode
    return ::ioctlsocket(socket, FIONBIO, &mode) == 0 ? 0 : -1;
}
int delameta_detail_set_blocking(int socket) {
    u_long mode = 0; // 0 = non-blocking mode
    return ::ioctlsocket(socket, FIONBIO, &mode) == 0 ? 0 : -1;
}
bool delameta_detail_is_fd_alive(int fd) {
    return _get_osfhandle(fd) != -1;
}
std::string delameta_detail_strerror(int errorCode) {
    char* messageBuffer = nullptr;

    int size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr
    );

    std::string errorMessage;
    if (size > 0 && messageBuffer) {
        errorMessage = messageBuffer;
        LocalFree(messageBuffer); // Free the allocated buffer
    } else {
        errorMessage = "Unknown error code: " + std::to_string(errorCode);
    }

    return errorMessage;
}

static Error last_error(bool is_wsa) {
    int errorCode = is_wsa ? WSAGetLastError() : GetLastError();
    return Error{errorCode, delameta_detail_strerror(errorCode)};
}

bool delameta_detail_is_socket_alive(int socket) {
    char dummy;
    int res = ::recv(socket, &dummy, 1, MSG_PEEK);
    if (res == 0)
        return false; // connection close
    if (res > 0)
        return true; // data available, socket is alive
    if (auto errno_ = WSAGetLastError(); res < 0 && (errno_ == WSAEWOULDBLOCK || errno_ == WSAEINPROGRESS))
        return true; // no data available, socket is alive
    return false;
}

auto delameta_detail_get_ip(int socket) -> std::string {
    sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (::getpeername(socket, (sockaddr*)&addr, &addr_len) != 0) {
        PANIC(
            fmt::format("Cannot resolve IP address of socket {}: {}", socket, last_error(true).what)
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
            fmt::format("Cannot convert struct IP into string IP: {}", last_error(true).what)
        );
    }

    return std::string(ip_str);
}

auto delameta_detail_get_filename(int fd) -> std::string {
    HANDLE hFile = (HANDLE)_get_osfhandle(fd);
    if (hFile == INVALID_HANDLE_VALUE) {
        PANIC(fmt::format("Cannot resolve filename from FD {}: Invalid handle", fd));
        return "<Invalid fd>";
    }

    char filename[MAX_PATH];
    DWORD dwSize = GetFinalPathNameByHandleA(hFile, filename, MAX_PATH, VOLUME_NAME_DOS);
    if (dwSize == 0) {
        PANIC(fmt::format("Cannot resolve filename from FD {}: Error {}", fd, GetLastError()));
        return "<Invalid fd>";
    }
    return std::string(filename);
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

    std::scoped_lock<std::mutex> lock(windows_socket_mutex);

    if (windows_socket_startup_counter == 0) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return Err(-1);
        }
        windows_socket_startup_counter++;
    }

    auto wsa_defer = etl::defer | [&]() {
        windows_socket_startup_counter--;
        if (windows_socket_startup_counter == 0) {
            WSACleanup();
        }
    };

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

auto delameta_detail_read(const char* file, int line, int fd, [[maybe_unused]] void* ssl, int timeout, bool(*is_alive)(int), bool is_wsa) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    u_long bytes_available = 0;

#ifndef DELAMETA_DISABLE_OPENSSL
    auto ssl_ = reinterpret_cast<SSL*>(ssl);
#endif

    while (is_alive(fd)) {
        if (get_bytes_available(fd, is_wsa, bytes_available) == -1) {
            return log_err(file, line, fd, last_error(is_wsa));
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
        auto size = ssl ? SSL_read(ssl_, buffer.data(), bytes_available) : is_wsa ? ::recv(fd, (char*)buffer.data(), bytes_available, 0) : ::_read(fd, buffer.data(), bytes_available);
#else
        auto size = is_wsa ? ::recv(fd, (char*)buffer.data(), bytes_available, 0) : ::_read(fd, buffer.data(), bytes_available);
#endif
        if (size < 0) {
            return log_err(file, line, fd, last_error(is_wsa));
        }

        buffer.resize(size);
        return log_received_ok(file, line, fd, buffer);
    }

    return log_err(file, line, fd, Error::ConnectionClosed);
}

auto delameta_detail_recvfrom(const char* file, int line, int fd, int timeout, void *peer) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    u_long bytes_available = 0;

    while (delameta_detail_is_socket_alive(fd)) {
        if (get_bytes_available(fd, true, bytes_available) == -1) {
            return log_err(file, line, fd, last_error(true));
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
            return log_err(file, line, fd, last_error(true));
        }

        buffer.resize(size);
        return log_received_ok(file, line, fd, buffer);
    }

    return log_err(file, line, fd, Error::ConnectionClosed);
}

auto delameta_detail_read_until(const char* file, int line, int fd, [[maybe_unused]] void* ssl, int timeout, bool(*is_alive)(int), bool is_wsa, size_t n) -> Result<std::vector<uint8_t>> {
#ifndef DELAMETA_DISABLE_OPENSSL
    if (ssl) { // cannot read parsial data
        return delameta_detail_read(file, line, fd, ssl, timeout, is_alive);
    }
#endif

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> buffer(n);

    int remaining_size = n;
    u_long bytes_available = 0;
    auto ptr = buffer.data();

    while (is_alive(fd)) {
        if (get_bytes_available(fd, is_wsa, bytes_available) == -1) {
            return log_err(file, line, fd, last_error(is_wsa));
        }

        if (bytes_available == 0) {
            if (timeout >= 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, fd, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto expected_length = std::min((int)bytes_available, remaining_size);
        auto size = is_wsa ? ::recv(fd, (char*)ptr, expected_length, 0) : ::_read(fd, ptr, expected_length);
        if (size < 0) {
            return log_err(file, line, fd, last_error(is_wsa));
        }

        static size_t debug_total_size;
        if (not is_wsa) {
            debug_total_size += size;
        }

        ptr += size;
        remaining_size -= size;

        if (size == 0) {
            buffer.resize(n - remaining_size);
            return log_received_ok(file, line, fd, buffer);
        }

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
    u_long bytes_available = 0;

    while (delameta_detail_is_socket_alive(fd)) {
        if (get_bytes_available(fd, true, bytes_available) == -1) {
            return log_err(file, line, fd, last_error(true));
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
            return log_err(file, line, fd, last_error(true));
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

auto delameta_detail_write(const char* file, int line, int fd, [[maybe_unused]] void* ssl, int timeout, bool(*is_alive)(int), bool is_wsa, std::string_view data) -> Result<void> {
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
        auto sent = ssl ? SSL_write(ssl_, &data[i], n) : is_wsa ? ::send(fd, &data[i], n, 0) : ::_write(fd, &data[i], n);
#else
        auto sent = is_wsa ? ::send(fd, &data[i], n, 0) : ::_write(fd, &data[i], n);
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
            if (is_wsa) {
                auto errno_ = WSAGetLastError();
                if (errno_ == WSAEWOULDBLOCK || errno_ == WSAEINPROGRESS) {
                    std::this_thread::sleep_for(10ms);
                    continue; // maybe try again
                }
                return log_err(file, line, fd, Error(errno_, delameta_detail_strerror(errno_)));
            } else {
                auto errno_ = GetLastError();
                if (errno_ == ERROR_IO_PENDING || errno_ == ERROR_TIMEOUT) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue; // Retry
                }
                return log_err(file, line, fd, Error(errno_, delameta_detail_strerror(errno_)));
            }
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
            return log_err(file, line, fd, last_error(true));
        }

        total += sent;
        i += sent;
    }

    return log_sent_ok(file, line, fd, total);
}

auto delameta_detail_create_socket(void* hint, const LogError& log_error) -> Result<int> {
    std::scoped_lock<std::mutex> lock(windows_socket_mutex);

    if (windows_socket_startup_counter == 0) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return Err(log_error.wsa());
        }
        windows_socket_startup_counter++;
    }

    auto h = reinterpret_cast<struct addrinfo*>(hint);

    SOCKET socket = ::socket(h->ai_family, h->ai_socktype, h->ai_protocol);
    if (socket == INVALID_SOCKET) {
        windows_socket_startup_counter--;
        if (windows_socket_startup_counter == 0) {
            WSACleanup();
        }
        return Err(log_error.wsa());
    }

    delameta_detail_set_non_blocking(socket);

    int enable = 1;
    if (::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&enable), sizeof(enable)) == SOCKET_ERROR) {
        ::closesocket(socket);
        windows_socket_startup_counter--;
        if (windows_socket_startup_counter == 0) {
            WSACleanup();
        }
        return Err(log_error.wsa());
    }

    return Ok(socket);
}

void delameta_detail_close_socket(int socket) {
    std::scoped_lock<std::mutex> lock(windows_socket_mutex);
    ::closesocket(socket);
    windows_socket_startup_counter--;
    if (windows_socket_startup_counter == 0) {
        WSACleanup();
    }
}

auto delameta_detail_windows_serial_read(const char* file, int line, void* fd, int timeout) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    HANDLE hSerial = fd;

    while (true) {
        // Check the status of the serial port
        DWORD errors;
        COMSTAT status;
        if (!ClearCommError(hSerial, &errors, &status)) {
            return log_err(file, line, Error(GetLastError(), "Failed to clear communication errors"));
        }

        // Check if bytes are available to read
        if (status.cbInQue == 0) {
            // Check for timeout
            if (timeout >= 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, Error::TransferTimeout);
            }

            // Sleep briefly before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Allocate a buffer and read the available data
        std::vector<uint8_t> buffer(status.cbInQue);
        DWORD bytesRead = 0;
        if (!ReadFile(hSerial, buffer.data(), status.cbInQue, &bytesRead, NULL)) {
            return log_err(file, line, Error(GetLastError(), "Failed to read from serial port"));
        }

        // Resize the buffer to the actual number of bytes read
        buffer.resize(bytesRead);
        return log_received_ok(file, line, buffer);
    }

    return log_err(file, line, Error::ConnectionClosed);
}

auto delameta_detail_windows_serial_read_until(const char* file, int line, void* fd, int timeout, size_t n) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> buffer(n);
    HANDLE hSerial = fd;

    size_t remaining_size = n;
    auto ptr = buffer.data();

    while (true) {
        // Check for available bytes in the input buffer
        DWORD errors;
        COMSTAT status;
        if (!ClearCommError(hSerial, &errors, &status)) {
            return log_err(file, line, Error(GetLastError(), "Failed to clear communication errors"));
        }

        DWORD bytes_available = status.cbInQue;

        if (bytes_available == 0) {
            // Check for timeout
            if (timeout >= 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(file, line, Error::TransferTimeout);
            }

            // Sleep briefly and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Read the available data
        DWORD bytes_to_read = std::min(static_cast<DWORD>(bytes_available), static_cast<DWORD>(remaining_size));
        DWORD bytes_read = 0;

        if (!ReadFile(hSerial, ptr, bytes_to_read, &bytes_read, NULL)) {
            return log_err(file, line, Error(GetLastError(), "Failed to read from serial port"));
        }

        ptr += bytes_read;
        remaining_size -= bytes_read;

        // Check if we have read the required number of bytes
        if (remaining_size == 0) {
            return log_received_ok(file, line, buffer);
        }
    }

    return log_err(file, line, Error::ConnectionClosed);
}

auto delameta_detail_windows_serial_write(const char* file, int line, void* fd, int timeout, std::string_view data) -> Result<void> {
    (void)timeout;
    size_t total = 0;
    HANDLE hSerial = fd;

    for (size_t i = 0; i < data.size();) {
        // Calculate the chunk size to send
        DWORD n = static_cast<DWORD>(std::min<size_t>(MAX_HANDLE_SZ, data.size() - i));
        DWORD bytes_written = 0;

        // Write data to the serial port
        if (!WriteFile(hSerial, &data[i], n, &bytes_written, NULL)) {
            DWORD last_error = GetLastError();
            if (last_error == ERROR_IO_PENDING || last_error == ERROR_TIMEOUT) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue; // Retry
            }
            return log_err(file, line, Error(last_error, "Failed to write to serial port"));
        }

        if (bytes_written == 0) {
            return log_err(file, line, Error::ConnectionClosed);
        }

        total += bytes_written;
        i += bytes_written;
    }

    return log_sent_ok(file, line, total);
}


Error LogError::wsa() const {
    return operator()(WSAGetLastError(), delameta_detail_strerror);
}

Error LogError::non_wsa() const {
    return operator()(GetLastError(), delameta_detail_strerror);
}

