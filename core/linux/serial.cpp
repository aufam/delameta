#include "delameta/serial.h"
#include "helper.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <termios.h>
#include <dirent.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

static Result<int> get_baudrate(const char* file, int line, int baud) {
    switch (baud) {
        case 50: return Ok(B50);
        case 75: return Ok(B75);
        case 110: return Ok(B110);
        case 134: return Ok(B134);
        case 150: return Ok(B150);
        case 200: return Ok(B200);
        case 300: return Ok(B300);
        case 600: return Ok(B600);
        case 1200: return Ok(B1200);
        case 1800: return Ok(B1800);
        case 2400: return Ok(B2400);
        case 4800: return Ok(B4800);
        case 9600: return Ok(B9600);
        case 19200: return Ok(B19200);
        case 38400: return Ok(B38400);
        case 59600: return Ok(B9600);
        case 57600: return Ok(B57600);
        case 115200: return Ok(B115200);
        case 230400: return Ok(B230400);
        case 460800: return Ok(B460800);
        case 500000: return Ok(B500000);
        case 576000: return Ok(B576000);
        case 921600: return Ok(B921600);
        case 1000000: return Ok(B1000000);
        case 1152000: return Ok(B1152000);
        case 1500000: return Ok(B1500000);
        case 2000000: return Ok(B2000000);
        case 2500000: return Ok(B2500000);
        case 3000000: return Ok(B3000000);
        case 3500000: return Ok(B3500000);
        case 4000000: return Ok(B4000000);
        default: {
            std::string what = "Cannot convert baudrate: " + std::to_string(baud);
            warning(file, line, what);
            return Err(Error{-1, what});
        }
    }
}

static auto log_errno(const char* file, int line) {
    int code = errno;
    std::string what = ::strerror(code);
    warning(file, line, what);
    return Err(Error{code, std::move(what)});
}

// TODO
struct FileDescriptorHandler {
    std::string filename;
    int fd;
    int counter;
};

static std::mutex handlers_mtx;
static std::list<FileDescriptorHandler> handlers;

auto Serial::Open(const char* file, int line, Args args) -> Result<Serial> {
    if (args.port == "auto") {
        struct dirent *ent;
        auto dir = ::opendir("/dev/");
        if (!dir) {
            return log_errno(file, line);
        }   
        while ((ent = ::readdir(dir)) != nullptr) {
            if (::strstr(ent->d_name, "ttyACM") != nullptr || ::strstr(ent->d_name, "ttyUSB") != nullptr) 
                args.port = std::string("/dev/") + ent->d_name;
        }
        ::closedir(dir);
    } 

    int fd = ::open(args.port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return log_errno(file, line);

    struct termios tty = {};
    if (::tcgetattr(fd, &tty) != 0)
        return log_errno(file, line);
    
    auto [baud_val, baud_err] = get_baudrate(file, line, args.baud);
    if (baud_err) return Err(std::move(*baud_err));

    ::cfsetispeed(&tty, *baud_val);
    ::cfsetospeed(&tty, *baud_val);
    
    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;                 /* 8-bit characters */
    tty.c_cflag &= ~(PARENB | PARODD);  /* no parity bit */
    tty.c_cflag &= ~CSTOPB;             /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;            /* no hardware flow control */

    // /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | IXOFF | IXON | IXANY | INLCR | ICRNL);
    tty.c_lflag = 0; // no signaling chars, no echo, no canonical processing
    tty.c_oflag = ~OPOST; // no remapping

    tty.c_cc[VTIME] = 1; // 100ms
    tty.c_cc[VMIN]  = 0; 

    if (::tcsetattr(fd, TCSANOW, &tty) != 0)
        return log_errno(file, line);

    ::tcflush(fd, TCIOFLUSH); // clear rx buffer

    info(file, line, "Created fd: " + std::to_string(fd));
    return Ok(Serial(file, line, fd, args.timeout));
}

Serial::Serial(const char* file, int line, int fd, int timeout) 
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , fd(fd)
    , timeout(timeout)
    , file(file)
    , line(line) { delameta_detail_set_non_blocking(fd); }

Serial::Serial(Serial&& other) 
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , fd(std::exchange(other.fd, -1))
    , timeout(other.timeout)
    , file(other.file)
    , line(other.line) {}

Serial::~Serial() {
    if (fd < 0) return;
    info(file, line, "Closed FD: " + std::to_string(fd));
    ::close(fd);
    fd = -1;
}

auto Serial::read() -> Result<std::vector<uint8_t>> {
    return delameta_detail_read(file, line, fd, nullptr, timeout, &delameta_detail_is_fd_alive);
}

auto Serial::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    return delameta_detail_read_until(file, line, fd, nullptr, timeout, &delameta_detail_is_fd_alive, n);
}

auto Serial::read_as_stream(size_t n) -> Stream {
    return delameta_detail_read_as_stream(file, line, timeout, this, n);
}

auto Serial::write(std::string_view data) -> Result<void> {
    return delameta_detail_write(file, line, fd, nullptr, timeout, &delameta_detail_is_fd_alive, data);
}

auto Serial::wait_until_ready() -> Result<void> {
    // TODO
    return Ok();
}

auto Server<Serial>::start(const char* file, int line, Serial::Args args) -> Result<void> {
    auto [ser, ser_err] = Serial::Open(file, line, std::move(args));
    if (ser_err) return Err(std::move(*ser_err));

    std::list<std::thread> threads;
    std::mutex mtx;
    std::atomic_bool is_running {true};

    on_stop = [this, &threads, &mtx, &is_running]() { 
        std::lock_guard<std::mutex> lock(mtx);
        is_running = false;
        for (auto& thd : threads) if (thd.joinable()) {
            thd.join();
        }
        on_stop = {};
    };

    while (is_running and delameta_detail_is_fd_alive(ser->fd)) {
        ser->wait_until_ready();
        auto read_result = ser->read();
        if (read_result.is_err()) {
            break;
        }

        std::lock_guard<std::mutex> lock(mtx);
        threads.emplace_back([this, &ser, data=std::move(read_result.unwrap()), &threads, &mtx, &is_running]() mutable {
            auto stream = execute_stream_session(*ser, delameta_detail_get_filename(ser->fd), data);
            stream >> *ser;

            // remove this thread from threads
            if (is_running) {
                std::lock_guard<std::mutex> lock(mtx);
                auto thd_id = std::this_thread::get_id();
                threads.remove_if([thd_id](std::thread& thd) { 
                    bool will_be_removed = thd.get_id() == thd_id;
                    if (will_be_removed) thd.detach();
                    return will_be_removed; 
                });
            }
        });
    }

    stop();
    return Ok();
}

void Server<Serial>::stop() {
    if (on_stop) {
        on_stop();
    }
}
