#include "delameta/serial/server.h"
#include <dirent.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <thread>
#include <mutex>
#include <atomic>
#include "../helper.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;

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

auto delameta_detail_create_serial(const char* file, int line, std::string port, int baud) -> Result<FileDescriptor> {
    auto log_errno = [file, line]() {
        int code = errno;
        std::string what = ::strerror(code);
        warning(file, line, what);
        return Err(Error{code, what});
    };

    if (port == "auto") {
        struct dirent *ent;
        auto dir = ::opendir("/dev/");
        if (!dir) {
            return log_errno();
        }   
        while ((ent = ::readdir(dir)) != nullptr) {
            if (::strstr(ent->d_name, "ttyACM") != nullptr || ::strstr(ent->d_name, "ttyUSB") != nullptr) 
                port = std::string("/dev/") + ent->d_name;
        }
        ::closedir(dir);
    } 

    auto [serial, err] = FileDescriptor::Open(file, line, port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (err) return Err(std::move(*err));

    struct termios tty = {};
    if (::tcgetattr(serial->fd, &tty) != 0)
        return log_errno();
    
    auto [baud_val, baud_err] = get_baudrate(file, line, baud);
    if (baud_err) return Err(std::move(*err));

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

    if (::tcsetattr(serial->fd, TCSANOW, &tty) != 0)
        return log_errno();

    ::tcflush(serial->fd, TCIOFLUSH); // clear rx buffer
    return Ok(std::move(*serial));
}

auto serial::Server::New(const char* file, int line, Args args) -> Result<Server> {
    return delameta_detail_create_serial(file, line, args.port, args.baud).then([&](FileDescriptor serial) {
        info(file, line, "Created serial server: " + std::to_string(serial.fd));
        return Server(std::move(serial));
    });
}

serial::Server::Server(FileDescriptor&& fd) 
    : StreamSessionServer({})
    , fd(std::move(fd)) {}

serial::Server::Server(Server&& other) 
    : StreamSessionServer(std::move(other.handler))
    , fd(std::move(other.fd))
    , on_stop(std::move(other.on_stop)) {}

serial::Server::~Server() {
    stop();
}

auto serial::Server::start() -> Result<void> {
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
 
    while (is_running and delameta_detail_is_fd_alive(fd.fd)) {
        auto read_result = fd.read();
        if (read_result.is_err()) {
            break;
        }

        std::lock_guard<std::mutex> lock(mtx);
        threads.emplace_back([this, data=std::move(read_result.unwrap()), &threads, &mtx, &is_running]() mutable {
            auto stream = execute_stream_session(fd, delameta_detail_get_filename(fd.fd), data);
            stream >> fd;

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
    return etl::Ok();
}

void serial::Server::stop() {
    if (on_stop) {
        on_stop();
    }
}