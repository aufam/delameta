#include "delameta/tcp.h"
#include "helper.h"
#include <cerrno>
#include <cstring>
#include <unordered_set>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Windows headers and definitions
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#undef min
#undef max
#define SHUT_RDWR SD_BOTH

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
        void *addr;

        // Check if the address is IPv4 or IPv6
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        ::inet_ntop(hint->ai_family, addr, ip_str, sizeof ip_str);
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
            auto errno_ = WSAGetLastError();
            if (errno_ != WSAEWOULDBLOCK && errno_ != WSAEINPROGRESS) {
                delameta_detail_close_socket(socket);
                err = log_error(errno_, delameta_detail_strerror);
                continue;
            }

            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(socket, &write_fds);

            struct timeval tv;
            tv.tv_sec = args.connection_timeout > 0 ? args.connection_timeout : 0;
            tv.tv_usec = 0;

            if (::select(socket + 1, nullptr, &write_fds, nullptr, &tv) <= 0) {
                delameta_detail_close_socket(socket);
                err = log_error.wsa();
                continue;
            }

            // Connection established or error occurred
            int error_code = 0;
            socklen_t len = sizeof(error_code);
            if (::getsockopt(socket, SOL_SOCKET, SO_ERROR, (char*)&error_code, &len) != 0 || error_code != 0) {
                delameta_detail_close_socket(socket);
                err = log_error.wsa();
                continue;
            }
        }

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
    , line(line)
{
    delameta_detail_set_non_blocking(socket);
    info(file, line, delameta_detail_log_format_fd(socket, "created"));
}

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
        info(file, line, delameta_detail_log_format_fd(socket, "closed"));
        delameta_detail_close_socket(socket);
        socket = -1;
    }
}

auto TCP::read() -> Result<std::vector<uint8_t>> {
    return delameta_detail_read(file, line, socket, nullptr, timeout, delameta_detail_is_socket_alive, true);
}

auto TCP::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    return delameta_detail_read_until(file, line, socket, nullptr, timeout, delameta_detail_is_socket_alive, true, n);
}

auto TCP::read_as_stream(size_t n) -> Stream {
    return delameta_detail_read_as_stream(file, line, timeout, this, n);
}

auto TCP::write(std::string_view data) -> Result<void> {
    return delameta_detail_write(file, line, socket, nullptr, timeout, delameta_detail_is_socket_alive, true, data);
}

auto Server<TCP>::start(const char* file, int line, Args args) -> Result<void> {
    if (args.max_socket <= 0) {
        return Err("Invalid max socket value, must be positive integer");
    }

    LogError log_error{file, line};
    auto [resolve, resolve_err] = delameta_detail_resolve_domain(args.host, SOCK_STREAM, true);
    if (resolve_err) return Err(log_error(*resolve_err, ::gai_strerror));

    auto hint = *resolve;
    auto defer_hint = defer | [hint]() { ::freeaddrinfo(hint); };

    auto [sock, sock_err] = delameta_detail_create_socket(hint, log_error);
    if (sock_err) return Err(std::move(*sock_err));

    auto socket = *sock;
    auto defer_socket = defer | [socket]() { delameta_detail_close_socket(socket); };

    if (::bind(socket, hint->ai_addr, hint->ai_addrlen) < 0) {
        return Err(log_error.wsa());
    }

    if (::listen(socket, args.max_socket) < 0) {
        return Err(log_error.wsa());
    }

    // TODO: event handler in MinGW
    // WSAEVENT event = WSACreateEvent();
    // if (event == WSA_INVALID_EVENT) {
    //     return Err(log_error.wsa());
    // }

    // auto defer_epoll = defer | [event]() { WSACloseEvent(event); };
    // if (WSAEventSelect(socket, event, FD_ACCEPT) == SOCKET_ERROR) {
    //     return Err(log_error.wsa());
    // }

    std::vector<std::thread> threads;
    std::unordered_set<int> client_set;
    std::deque<int> client_que;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic_bool is_running {true};

    on_stop = [this, &is_running, &cv]() { 
        is_running = false;
        cv.notify_all();
        on_stop = {};
    };

    auto work = [this, file, line, &args, &is_running, &mtx, &cv, &client_set, &client_que](int idx) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return;
        }

        auto wsa_defer = etl::defer | &WSACleanup;

        info(file, line, "Spawned worker thread: " + std::to_string(idx));
        while (is_running) {
            int sock_client;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock);
                if (not is_running) break;
                sock_client = client_que.front();
                client_que.pop_front();
            }

            info(file, line, "processing in thread " + std::to_string(idx) + ", socket = " + std::to_string(sock_client));

            TCP session(file, line, sock_client, args.timeout);
            session.keep_alive = args.keep_alive;

            for (int cnt = 1; is_running and delameta_detail_is_socket_alive(sock_client); ++cnt) {
                auto received_result = session.read(); // TODO: read() doesn't check for `is_running`
                if (received_result.is_err()) {
                    break;
                }

                auto stream = this->execute_stream_session(session, delameta_detail_get_ip(session.socket), received_result.unwrap());
                stream >> session;

                if (not session.keep_alive) {
                    if (session.max > 0 and cnt >= session.max) {
                        info(file, line, delameta_detail_log_format_fd(sock_client, "reached maximum receive"));
                    }
                    break;
                }

                info(file, line, delameta_detail_log_format_fd(sock_client, "kept alive"));
            }

            // shutdown if still connected
            if (delameta_detail_is_socket_alive(session.socket)) {
                ::shutdown(session.socket, SHUT_RDWR);
                info(file, line, delameta_detail_log_format_fd(sock_client, "closed by server"));
            } else {
                info(file, line, delameta_detail_log_format_fd(sock_client, "closed by peer"));
            }

            {
                std::lock_guard<std::mutex> lock(mtx);
                client_set.erase(sock_client);
            }
        }
    };

    threads.reserve(args.max_socket);
    client_set.reserve(args.max_socket);
    for (int i = 0; i < args.max_socket; ++i) {
        threads.emplace_back(work, i);
    }

    while (is_running) {
        // TODO: event handler in MinGW?
        // DWORD dwEventIndex = WSAWaitForMultipleEvents(1, &event, FALSE, 10, FALSE);
        // if (dwEventIndex == WAIT_OBJECT_0)
        std::this_thread::sleep_for(10ms);
        {
            int new_sock_client = ::accept(socket, nullptr, nullptr);
            if (new_sock_client < 0) {
                auto errno_ = WSAGetLastError();
                if (errno_ != WSAEWOULDBLOCK && errno_ != WSAEINPROGRESS) {
                    WARNING(delameta_detail_log_format_fd(socket, "accept() failed, ") + delameta_detail_strerror(errno_));
                }
                continue;
            }

            std::lock_guard<std::mutex> lock(mtx);
            if ((int)client_set.size() >= args.max_socket) {
                ::shutdown(new_sock_client, SHUT_RDWR);
                WARNING(delameta_detail_log_format_fd(socket, "Thread pool is full"));
                continue;
            }

            auto [_, ok] = client_set.insert(new_sock_client);
            if (not ok) {
                WARNING(delameta_detail_log_format_fd(new_sock_client, "Duplicate socket"));
                continue;
            }

            client_que.push_back(new_sock_client);
            cv.notify_one(); // notify thread pool
        }
    }

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
