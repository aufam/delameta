#include "delameta/socket.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <thread>
#include "delameta.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

static auto log_errno(const char* file, int line) {
    int code = errno;
    std::string what = ::strerror(code);
    warning(file, line, what);
    return Err(Error{code, std::move(what)});
};

static auto log_err(const char* file, int line, Socket* s, Error err) {
    warning(file, line, "Socket " + std::to_string(s->socket) + ": Error: " + err.what);
    return Err(err);
};

static auto log_received_ok(const char* file, size_t line, Socket* s, std::vector<uint8_t>& res) {
    info(file, line, "Socket " + std::to_string(s->socket) + " received " + std::to_string(res.size()) + " bytes");
    return Ok(std::move(res));
};

static auto log_sent_ok(const char* file, size_t line, Socket* s, size_t n) {
    info(file, line, "Socket " + std::to_string(s->socket) + " sent " + std::to_string(n) + " bytes");
    return Ok();
};

auto Socket::New(const char* file, int line, int __domain, int __type, int __protocol) -> Result<Socket> {
    int socket = ::socket(__domain, __type, __protocol);
    if (socket < 0) {
        return log_errno(file, line);
    } else {
        info(file, line, "Created socket: " + std::to_string(socket));
        return Ok(Socket(file, line, socket));
    }
}

auto Socket::Accept(const char* file, int line, int __fd, void* __addr, void* __addr_len) -> Result<Socket> {
    int socket = ::accept(__fd, (sockaddr *__restrict__)__addr, (socklen_t *__restrict__)__addr_len);
    if (socket < 0) {
        return Err(Error{errno, ::strerror(errno)});
    } else {
        info(file, line, "Accepted socket: " + std::to_string(socket));
        return Ok(Socket(file, line, socket));
    }
}

Socket::Socket(const char* file, int line, int socket) 
    : delameta::Descriptor()
    , socket(socket)
    , keep_alive(true)
    , timeout(-1)
    , max(-1) 
    , file(file)
    , line(line) { delameta_detail_set_non_blocking(socket); }

Socket::Socket(Socket&& other) 
    : delameta::Descriptor()
    , socket(std::exchange(other.socket, -1))
    , keep_alive(other.keep_alive)
    , timeout(other.timeout)
    , max(other.max) 
    , file(other.file)
    , line(other.line) {}

Socket::~Socket() {
    if (socket >= 0) {
        info(file, line, "Closed socket: " + std::to_string(socket));
        ::close(socket);
        socket = -1;
    }
}

auto Socket::read() -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res;
    bool retried = false;
    auto start = std::chrono::high_resolution_clock::now();

    while (delameta_detail_is_socket_alive(socket)) {
        std::vector<uint8_t> buffer(MAX_HANDLE_SZ);
        auto size = ::recv(socket, buffer.data(), MAX_HANDLE_SZ, 0);

        if (size < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (not retried) {
                    // the incoming data might exceed MAX_HANDLE_SZ 
                    retried = true;
                } else {
                    // the incoming data might be exactly MAX_HANDLE_SZ or its multiply
                    if (not res.empty()) {
                        return log_received_ok(__FILE__, __LINE__, this, res);
                    }

                    // try again until get some data or timeout
                    if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                        return log_err(__FILE__, __LINE__, this, Error::TransferTimeout);
                    }
                }
                std::this_thread::sleep_for(10ms);
                continue;
            } else {
                return log_err(__FILE__, __LINE__, this, Error(errno, ::strerror(errno)));
            }
        } else if (size == 0) {
            return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
        }

        res.insert(res.end(), buffer.begin(), buffer.begin() + size);
        if (size == MAX_HANDLE_SZ) {
            retried = false;
            continue;
        } else {
            return log_received_ok(__FILE__, __LINE__, this, res);
        }
    }

    return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
}

auto Socket::receive_until(size_t n) -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res;
    res.reserve(n);
    bool retried = false;
    auto start = std::chrono::high_resolution_clock::now();

    while (delameta_detail_is_socket_alive(socket)) {
        size_t current_size = res.size();
        size_t remaining_size = n - current_size;

        if (remaining_size == 0) {
            return log_received_ok(__FILE__, __LINE__, this, res);
        }

        std::vector<uint8_t> buffer(remaining_size);
        auto size = ::recv(socket, buffer.data(), remaining_size, 0);

        if (size < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                    return log_err(__FILE__, __LINE__, this, Error::TransferTimeout);
                }
                std::this_thread::sleep_for(10ms);
                continue;
            } else {
                return log_err(__FILE__, __LINE__, this, Error(errno, ::strerror(errno)));
            }
        } else if (size == 0) {
            return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
        }

        // Append received data to the result buffer
        res.insert(res.end(), buffer.begin(), buffer.begin() + size);
    }

    return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
}

auto Socket::read_as_stream(size_t n) -> Stream {
    Stream s;
    for (int total = n; total > 0;) {
        int size = std::min(total, MAX_HANDLE_SZ);
        s << [this, size, buffer=std::vector<uint8_t>{}]() mutable -> std::string_view {
            auto data = this->receive_until(size);
            if (data.is_ok()) {
                buffer = std::move(data.unwrap());
            }
            return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
        };
        total -= size;
    }

    return s;
}

auto Socket::write(std::string_view data) -> Result<void> {
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        auto n = std::min<size_t>(MAX_HANDLE_SZ, data.size() - i);
        auto sent = ::send(socket, &data[i], n, 0);
        
        if (sent == 0) {
            return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
        } else if (sent < 0) {
            return log_errno(__FILE__, __LINE__);
        }

        total += sent;
        i += sent;
    }

    return log_sent_ok(__FILE__, __LINE__, this, total);
}

