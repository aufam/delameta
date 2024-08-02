#include "delameta/socket/stream.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <thread>
#include "../delameta.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

static auto log_err(const char* file, int line) {
    int code = errno;
    std::string what = ::strerror(code);
    warning(file, line, what);
    return Err(Error{code, what});
};

static auto log_err(const char* file, int line, socket::Stream* s, Error err) {
    warning(file, line, "Socket " + std::to_string(s->socket) + ": Error: " + err.what);
    return Err(err);
};

static auto log_received_ok(const char* file, size_t line, socket::Stream* s, std::vector<uint8_t>& res) {
    info(file, line, "Socket " + std::to_string(s->socket) + " received " + std::to_string(res.size()) + " bytes");
    return Ok(std::move(res));
};

static auto log_sent_ok(const char* file, size_t line, socket::Stream* s, size_t n) {
    info(file, line, "Socket " + std::to_string(s->socket) + " sent " + std::to_string(n) + " bytes");
    return Ok();
};

auto socket::Stream::New(const char* file, int line, int __domain, int __type, int __protocol) -> Result<Stream> {
    int socket = ::socket(__domain, __type, __protocol);
    if (socket < 0) {
        return log_err(file, line);
    } else {
        info(file, line, "Created socket: " + std::to_string(socket));
        return Ok(Stream(file, line, socket));
    }
}

auto socket::Stream::Accept(const char* file, int line, int __fd, void* __addr, void* __addr_len) -> Result<Stream> {
    int socket = ::accept(__fd, (sockaddr *__restrict__)__addr, (socklen_t *__restrict__)__addr_len);
    if (socket < 0) {
        return Err(Error{errno, ::strerror(errno)});
    } else {
        info(file, line, "Accepted socket: " + std::to_string(socket));
        return Ok(Stream(file, line, socket));
    }
}

socket::Stream::Stream(const char* file, int line, int socket) 
    : delameta::Stream()
    , socket(socket)
    , keep_alive(true)
    , timeout(-1)
    , max(-1) 
    , file(file)
    , line(line) {
    delameta_detail_set_non_blocking(socket);
}

socket::Stream::Stream(Stream&& other) 
    : delameta::Stream(std::move(other))
    , socket(std::exchange(other.socket, -1))
    , keep_alive(other.keep_alive)
    , timeout(other.timeout)
    , max(other.max) 
    , file(other.file)
    , line(other.line) {}

socket::Stream::~Stream() {
    if (socket >= 0) {
        info(file, line, "Closed socket: " + std::to_string(socket));
        ::close(socket);
        socket = -1;
    }
}

auto socket::Stream::receive() -> Result<std::vector<uint8_t>> {
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

auto socket::Stream::receive_until(size_t n) -> Result<std::vector<uint8_t>> {
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

auto socket::Stream::send() -> Result<void> {
    size_t total = 0;
    Error* err = nullptr;

    *this >> [this, &err, &total](std::string_view buf) {
        if (err != nullptr) 
            return;
        
        for (size_t i = 0; i < buf.size();) {
            auto n = std::min<size_t>(MAX_HANDLE_SZ, buf.size() - i);
            auto sent = ::send(socket, &buf[i], n, 0);
            
            if (sent == 0) {
                err = new Error(Error::ConnectionClosed);
                return;
            } else if (sent < 0) {
                err = new Error(errno, ::strerror(errno));
                return;
            }

            total += sent;
            i += sent;
        }
    };

    if (err) {
        auto se = defer | [&]() { delete err; };
        return log_err(__FILE__, __LINE__, this, *err);
    } else {
        return log_sent_ok(__FILE__, __LINE__, this, total);
    }
}

