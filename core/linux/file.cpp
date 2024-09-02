#include "delameta/file.h"
#include "helper.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

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

auto File::Open(const char* file, int line, Args args) -> Result<File> {
    int __oflag = O_RDONLY;
    const char* __file = args.filename.c_str();

    if (args.mode == "r") {
        __oflag = O_RDONLY;
    } else if (args.mode == "w") {
        __oflag = O_WRONLY | O_TRUNC | O_CREAT;
    } else if (args.mode == "wa") {
        __oflag = O_WRONLY | O_APPEND | O_CREAT;
    } else if (args.mode == "rw") {
        __oflag = O_RDWR | O_TRUNC | O_CREAT;
    } else if (args.mode == "rwa") {
        __oflag = O_RDWR | O_APPEND | O_CREAT;
    } else {
        Error err = {-1, "Invalid mode"};
        warning(file, line, err.what);
        return Err(std::move(err));
    }

    int fd = __oflag & (O_WRONLY | O_RDWR) ? ::open(__file, __oflag, 0644) : ::open(__file, __oflag);
    if (fd < 0) {
        return log_errno(file, line);
    } else {
        info(file, line, "Created fd: " + std::to_string(fd));
        return Ok(File(file, line, fd));
    }
}

File::File(const char* file, int line, int fd) 
    : Descriptor()
    , fd(fd)
    , file(file)
    , line(line) {}

File::File(File&& other) 
    : Descriptor()
    , fd(std::exchange(other.fd, -1))
    , file(other.file)
    , line(other.line) {}

File::~File() {
    if (fd < 0) return;
    ::close(fd);
    fd = -1;
}

auto File::read() -> Result<std::vector<uint8_t>> {
    return delameta_detail_read(file, line, fd, -1, &delameta_detail_is_fd_alive);
}

auto File::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    return delameta_detail_read_until(file, line, fd, -1, &delameta_detail_is_fd_alive, n);
}

auto File::read_as_stream(size_t n) -> Stream {
    return delameta_detail_read_as_stream(file, line, fd, -1, this, n);
}

auto File::write(std::string_view data) -> Result<void> {
    return delameta_detail_write(file, line, fd, -1, &delameta_detail_is_fd_alive, data);
}

auto File::file_size() -> size_t {
    off_t cp = lseek(fd, 0, SEEK_CUR);
    if (cp == -1) {
        log_errno(file, line);
        return 0;
    }
    
    off_t size = lseek(fd, 0, SEEK_END);
    if (cp == -1) {
        log_errno(file, line);
        return 0;
    }

    if (lseek(fd, cp, SEEK_SET) == -1) {
        log_errno(file, line);
        return 0;
    }

    return size;
}

auto File::operator<<(Stream& other) -> File& {
    other >> *this;
    return *this;
}

auto File::operator>>(Stream& s) -> File& {
    auto total = file_size();

    s << [self=std::move(*this), total, buffer=std::vector<uint8_t>{}](Stream& s) mutable -> std::string_view {
        size_t n = std::min(total, (size_t)MAX_HANDLE_SZ);
        auto data = self.read_until(n);

        if (data.is_ok()) {
            buffer = std::move(data.unwrap());
            total -= n;
            s.again = total > 0;
        } else {
            buffer = {};
            warning(self.file, self.line, data.unwrap_err().what);
        }
        return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
    };

    return *this;
}
