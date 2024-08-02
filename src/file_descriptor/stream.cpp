#include "delameta/file_descriptor/stream.h"
#include <fcntl.h>
#include <unistd.h>
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

static auto log_errno(const char* file, int line) {
    int code = errno;
    std::string what = ::strerror(code);
    warning(file, line, what);
    return Err(Error{code, what});
};

static auto log_err(const char* file, int line, file_descriptor::Stream* s, Error err) {
    warning(file, line, "FD " + std::to_string(s->fd) + ": Error: " + err.what);
    return Err(err);
};

static auto log_received_ok(const char* file, size_t line, file_descriptor::Stream* s, std::vector<uint8_t>& res) {
    info(file, line, "FD " + std::to_string(s->fd) + " read " + std::to_string(res.size()) + " bytes");
    return Ok(std::move(res));
};

static auto log_sent_ok(const char* file, size_t line, file_descriptor::Stream* s, size_t n) {
    info(file, line, "FD " + std::to_string(s->fd) + " write " + std::to_string(n) + " bytes");
    return Ok();
};

auto file_descriptor::Stream::Open(const char* file, int line, const char* __file, int __oflag) -> Result<Stream> {
    int fd;
    if (__oflag & O_WRONLY) {
        fd = ::open(__file, __oflag, 0644);
    } else {
        fd = ::open(__file, __oflag);
    }
    if (fd < 0) {
        return log_errno(file, line);
    } else {
        info(file, line, "Created fd: " + std::to_string(fd));
        return Ok(Stream(file, line, fd));
    }
}

file_descriptor::Stream::Stream(const char* file, int line, int fd) : delameta::Stream(), fd(fd), timeout(-1), file(file), line(line) {
    delameta_detail_set_non_blocking(fd);
}

file_descriptor::Stream::Stream(Stream&& other) 
    : delameta::Stream(std::move(other))
    , fd(std::exchange(other.fd, -1))
    , timeout(other.timeout)
    , file(other.file)
    , line(other.line) {}

file_descriptor::Stream::~Stream() {
    if (fd >= 0) {
        auto msg = "Closed fd: " + std::to_string(fd);
        info(file, line, msg);
        ::close(fd);
        fd = -1;
    }
}

auto file_descriptor::Stream::read() -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res;
    bool retried = false;
    auto start = std::chrono::high_resolution_clock::now();

    while (delameta_detail_is_fd_alive(fd)) {
        std::vector<uint8_t> buffer(MAX_HANDLE_SZ);
        auto size = ::read(fd, buffer.data(), MAX_HANDLE_SZ);

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

auto file_descriptor::Stream::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res;
    res.reserve(n);
    bool retried = false;
    auto start = std::chrono::high_resolution_clock::now();

    while (delameta_detail_is_fd_alive(fd)) {
        size_t current_size = res.size();
        size_t remaining_size = n - current_size;

        if (remaining_size == 0) {
            return log_received_ok(__FILE__, __LINE__, this, res);
        }

        std::vector<uint8_t> buffer(remaining_size);
        auto size = ::read(fd, buffer.data(), remaining_size);

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

auto file_descriptor::Stream::write() -> Result<void> {
    size_t total = 0;
    Error* err = nullptr;

    *this >> [this, &err, &total](std::string_view buf) {
        if (err != nullptr) 
            return;
        
        for (size_t i = 0; i < buf.size();) {
            auto n = buf.size() - i;
            auto sent = ::write(fd, &buf[i], n);
            
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

auto file_descriptor::Stream::file_size() -> Result<size_t> {
    off_t cp = lseek(fd, 0, SEEK_CUR);
    if (cp == -1)
        return log_errno(file, line);
    
    off_t size = lseek(fd, 0, SEEK_END);
    if (cp == -1)
        return log_errno(file, line);

    if (lseek(fd, cp, SEEK_SET) == -1)
        return log_errno(file, line);

    return Ok(size_t(size));
}

template <>
Stream& Stream::operator<<(file_descriptor::Stream other) {
    auto [size, size_err] = other.file_size();
    if (size_err) {
        return *this;
    }

    auto p_stream = new file_descriptor::Stream(std::move(other));

    for (int total_size = int(*size); total_size > 0;) {
        size_t n = std::min(total_size, MAX_HANDLE_SZ);
        *this << [p_stream, n, buffer=std::vector<uint8_t>{}]() mutable -> std::string_view {
            auto data = p_stream->read_until(n);
            if (data.is_ok()) {
                buffer = std::move(data.unwrap());
            }
            return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
        };
        total_size -= n;
    }

    *this << [p_stream]() mutable -> std::string_view {
        delete p_stream;
        return "";
    };

    return *this;
}
