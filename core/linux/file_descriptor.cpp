#include "delameta/file_descriptor.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include "helper.h"

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

static auto log_err(const char* file, int line, FileDescriptor* s, Error err) {
    warning(file, line, "FD " + std::to_string(s->fd) + ": Error: " + err.what);
    return Err(err);
};

static auto log_received_ok(const char* file, size_t line, FileDescriptor* s, std::vector<uint8_t>& res) {
    info(file, line, "FD " + std::to_string(s->fd) + " read " + std::to_string(res.size()) + " bytes");
    return Ok(std::move(res));
};

static auto log_sent_ok(const char* file, size_t line, FileDescriptor* s, size_t n) {
    info(file, line, "FD " + std::to_string(s->fd) + " write " + std::to_string(n) + " bytes");
    return Ok();
};

struct FileDescriptorHandler {
    std::string filename;
    int fd;
    int counter;
};

static std::mutex handlers_mtx;
static std::list<FileDescriptorHandler> handlers;

auto FileDescriptor::Open(const char* file, int line, const char* __file, int __oflag) -> Result<FileDescriptor> {
    std::lock_guard<std::mutex> lock(handlers_mtx);
    auto it = std::find_if(handlers.begin(), handlers.end(), [__file](FileDescriptorHandler& h) {
        return h.filename == __file;
    });
    
    if (it != handlers.end()) {
        it->counter++;
        return Ok(FileDescriptor(file, line, it->fd));
    }

    int fd = __oflag & O_WRONLY ? ::open(__file, __oflag, 0644) : ::open(__file, __oflag);
    if (fd < 0) {
        return log_errno(file, line);
    } else {
        handlers.emplace_back(FileDescriptorHandler{__file, fd, 0});
        info(file, line, "Created fd: " + std::to_string(fd));
        return Ok(FileDescriptor(file, line, fd));
    }
}

FileDescriptor::FileDescriptor(const char* file, int line, int fd) 
    : Descriptor()
    , fd(fd)
    , timeout(-1)
    , file(file)
    , line(line) { delameta_detail_set_non_blocking(fd); }

FileDescriptor::FileDescriptor(FileDescriptor&& other) 
    : Descriptor()
    , fd(std::exchange(other.fd, -1))
    , timeout(other.timeout)
    , file(other.file)
    , line(other.line) {}

FileDescriptor::~FileDescriptor() {
    if (fd < 0) return;

    std::lock_guard<std::mutex> lock(handlers_mtx);
    auto it = std::find_if(handlers.begin(), handlers.end(), [this](FileDescriptorHandler& h) {
        return h.fd == fd;
    });

    if (it == handlers.end()) {
        panic(file, line, "Fatal Error: file descriptor not found in handler");
        return;
    }

    if (--it->counter == 0) {
        info(file, line, "Closed fd: " + std::to_string(fd));
        ::close(fd);
        handlers.erase(it);
    }
    fd = -1;
}

auto FileDescriptor::read() -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    int bytes_available = 0;

    while (delameta_detail_is_fd_alive(fd)) {
        if (::ioctl(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(__FILE__, __LINE__, this, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(__FILE__, __LINE__, this, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        std::vector<uint8_t> buffer(bytes_available);
        auto size = ::read(fd, buffer.data(), bytes_available);
        if (size < 0) {
            return log_err(__FILE__, __LINE__, this, Error(errno, ::strerror(errno)));
        }

        return log_received_ok(__FILE__, __LINE__, this, buffer);
    }

    return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
}

auto FileDescriptor::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> buffer(n);

    int remaining_size = n;
    int bytes_available = 0;
    auto ptr = buffer.data();

    while (delameta_detail_is_fd_alive(fd)) {
        if (::ioctl(fd, FIONREAD, &bytes_available) == -1) {
            return log_err(__FILE__, __LINE__, this, Error(errno, ::strerror(errno)));
        }

        if (bytes_available == 0) {
            if (timeout > 0 && std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout)) {
                return log_err(__FILE__, __LINE__, this, Error::TransferTimeout);
            }
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto size = ::read(fd, ptr, std::min(bytes_available, remaining_size));
        if (size < 0) {
            return log_err(__FILE__, __LINE__, this, Error(errno, ::strerror(errno)));
        }

        ptr += size;
        remaining_size -= size;

        if (remaining_size <= 0) {
            return log_received_ok(__FILE__, __LINE__, this, buffer);
        }
    }

    return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
}

auto FileDescriptor::read_as_stream(size_t n) -> Stream {
    Stream s;
    for (int total = n; total > 0;) {
        int size = std::min(total, MAX_HANDLE_SZ);
        s << [this, size, buffer=std::vector<uint8_t>{}]() mutable -> std::string_view {
            auto data = this->read_until(size);
            if (data.is_ok()) {
                buffer = std::move(data.unwrap());
            } else {
                warning(file, line, data.unwrap_err().what);
            }
            return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
        };
        total -= size;
    }

    return s;
}

auto FileDescriptor::write(std::string_view data) -> Result<void> {
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        if (!delameta_detail_is_fd_alive(fd)) {
            return log_err(__FILE__, __LINE__, this, Error::ConnectionClosed);
        }

        auto n = std::min<size_t>(MAX_HANDLE_SZ, data.size() - i);
        auto sent = ::write(fd, &data[i], n);
        
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

auto FileDescriptor::file_size() -> Result<size_t> {
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

auto FileDescriptor::operator<<(Stream& other) -> FileDescriptor& {
    other >> *this;
    return *this;
}

auto FileDescriptor::operator>>(Stream& s) -> FileDescriptor& {
    auto [size, size_err] = file_size();
    if (size_err) {
        return *this;
    }

    auto p_fd = new FileDescriptor(std::move(*this));

    for (int total_size = int(*size); total_size > 0;) {
        size_t n = std::min(total_size, MAX_HANDLE_SZ);
        s << [p_fd, n, buffer=std::vector<uint8_t>{}]() mutable -> std::string_view {
            auto data = p_fd->read_until(n);
            if (data.is_ok()) {
                buffer = std::move(data.unwrap());
            }
            return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
        };
        total_size -= n;
    }

    s << [p_fd]() mutable -> std::string_view {
        delete p_fd;
        return "";
    };

    return *this;
}
