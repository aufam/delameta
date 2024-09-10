#include "delameta/tcp.h"
#include "helper.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
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

auto TCP::Open(const char* file, int line, Args args) -> Result<TCP> {
    LogError log_error{file, line};
    auto [resolve, resolve_err] = delameta_detail_resolve_domain(args.host, SOCK_STREAM, false);
    if (resolve_err) return Err(log_error(*resolve_err, ::gai_strerror));

    auto hint = *resolve;
    auto se = defer | [hint]() { ::freeaddrinfo(hint); };
    Error err{-1, "Unable to resolve hostname: " + args.host};

    for (auto p = hint; p != nullptr; p = p->ai_next) {
        char ip_str[INET6_ADDRSTRLEN];
        void* addr_ptr = nullptr;
        void *addr;

        // Check if the address is IPv4 or IPv6
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        inet_ntop(hint->ai_family, addr, ip_str, sizeof ip_str);
        info(__FILE__, __LINE__, "resolved: " + std::string(ip_str));
    }

    for (auto p = hint; p != nullptr; p = p->ai_next) {
        auto [sock, sock_err] = delameta_detail_create_socket(p, log_error);
        if (sock_err) {
            err = std::move(*sock_err);
            continue;
        }

        auto socket = *sock;
        if (::connect(socket, p->ai_addr, p->ai_addrlen) != 0) {
            if (errno != EINPROGRESS) {
                ::close(socket);
                err = log_error(errno, ::strerror);
                continue;
            }

            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(socket, &write_fds);

            struct timeval tv;
            tv.tv_sec = args.connection_timeout > 0 ? args.connection_timeout : 0;
            tv.tv_usec = 0;

            if (::select(socket + 1, nullptr, &write_fds, nullptr, &tv) <= 0) {
                ::close(socket);
                err = log_error(errno, ::strerror);
                continue;
            }

            // Connection established or error occurred
            int error_code = 0;
            socklen_t len = sizeof(error_code);
            if (::getsockopt(socket, SOL_SOCKET, SO_ERROR, &error_code, &len) != 0 || error_code != 0){
                ::close(socket);
                err = log_error(errno, ::strerror);
                continue;
            }
        } 

        info(file, line, "Created socket as TCP client: " + std::to_string(socket));
        return Ok(TCP(file, line, socket, args.timeout));
    }

    return Err(std::move(err));
}

TCP::TCP(const char* file, int line, int socket, int timeout)
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , socket(socket)
    , keep_alive(true)
    , timeout(timeout)
    , max(-1) 
    , file(file)
    , line(line) { delameta_detail_set_non_blocking(socket); }

TCP::TCP(TCP&& other)
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , socket(std::exchange(other.socket, -1))
    , keep_alive(other.keep_alive)
    , timeout(other.timeout)
    , max(other.max) 
    , file(other.file)
    , line(other.line) {}

TCP::~TCP() {
    if (socket >= 0) {
        info(file, line, "Closed TCP socket: " + std::to_string(socket));
        ::close(socket);
        socket = -1;
    }
}

auto TCP::read() -> Result<std::vector<uint8_t>> {
    return delameta_detail_read(file, line, socket, nullptr, timeout, delameta_detail_is_socket_alive);
}

auto TCP::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    return delameta_detail_read_until(file, line, socket, nullptr, timeout, delameta_detail_is_socket_alive, n);
}

auto TCP::read_as_stream(size_t n) -> Stream {
    return delameta_detail_read_as_stream(file, line, timeout, this, n);
}

auto TCP::write(std::string_view data) -> Result<void> {
    return delameta_detail_write(file, line, socket, nullptr, timeout, delameta_detail_is_socket_alive, data);
}

auto Server<TCP>::start(const char* file, int line, Args args) -> Result<void> {
    LogError log_error{file, line};
    auto [resolve, resolve_err] = delameta_detail_resolve_domain(args.host, SOCK_STREAM, true);
    if (resolve_err) return Err(log_error(*resolve_err, ::gai_strerror));

    auto hint = *resolve;
    auto defer_hint = defer | [hint]() { ::freeaddrinfo(hint); };

    auto [sock, sock_err] = delameta_detail_create_socket(hint, log_error);
    if (sock_err) return Err(std::move(*sock_err));

    auto socket = *sock;
    auto defer_socket = defer | [socket]() { ::close(socket); };

    if (::bind(socket, hint->ai_addr, hint->ai_addrlen) < 0) {
        return Err(log_error(errno, ::strerror));
    }

    if (::listen(socket, args.max_socket) < 0) {
        return Err(log_error(errno, ::strerror));
    }

    info(file, line, "Created socket as TCP server: " + std::to_string(socket));

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

    auto work = [this, socket, file, line, &args, &is_running](int idx) {
        info(file, line, "Spawned worker thread: " + std::to_string(idx));
        while (is_running) {
            int sock_client = ::accept(socket, nullptr, nullptr);
            if (sock_client < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // no incoming connection, sleep briefly
                    std::this_thread::sleep_for(10ms);
                } else {
                    warning(__FILE__, __LINE__, strerror(errno));
                }
                continue;
            }

            TCP session(file, line, sock_client, 1);
            session.keep_alive = args.keep_alive;
        
            for (int cnt = 1; is_running and delameta_detail_is_socket_alive(sock_client); ++cnt) {
                auto received_result = session.read(); // TODO: read() doesn't check for is_running
                if (received_result.is_err()) {
                    if (received_result.unwrap_err().code == Error::TransferTimeout) continue;
                    else break;
                }

                DBG(info, std::string(received_result.unwrap().begin(), received_result.unwrap().end()));
                auto stream = this->execute_stream_session(session, delameta_detail_get_ip(session.socket), received_result.unwrap());
                stream >> session;

                if (not session.keep_alive) {
                    if (session.max > 0 and cnt >= session.max) {
                        warning(session.file, session.line, "Reached maximum receive: " + std::to_string(session.socket));
                    }
                    break;
                }
            }

            // shutdown if still connected
            if (delameta_detail_is_socket_alive(session.socket)) {
                ::shutdown(session.socket, SHUT_RDWR);
                info(file, line, "Closed by server: " + std::to_string(session.socket));
            } else {
                info(file, line, "Closed by peer: " + std::to_string(session.socket));
            }
        }
    };

    int i = 0;
    for (; i < args.max_socket - 1; ++i) {
        threads.emplace_back(work, i);
    }

    work(i);
    for (auto& thd : threads) if (thd.joinable()) {
        thd.join();
    }
    return etl::Ok();
}

void Server<TCP>::stop() {
    if (on_stop) {
        on_stop();
    }
}