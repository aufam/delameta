#include "delameta/udp.h"
#include "helper.h"
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>

#ifndef _WIN32
// Unix/Linux headers and definitions
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#else
// Windows headers and definitions
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#pragma comment(lib, "Ws2_32.lib")  // Link Winsock library
#undef min
#undef max
#define SHUT_RDWR SD_BOTH
#endif

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

auto UDP::Open(const char* file, int line, Args args) -> Result<UDP> {
    LogError log_error {file, line};

    auto [resolve, resolve_err] = delameta_detail_resolve_domain(args.host, SOCK_DGRAM, args.as_server);
    if (resolve_err) return Err(log_error(*resolve_err, ::gai_strerror));

    auto [sock, sock_err] = delameta_detail_create_socket(*resolve, log_error);
    if (sock_err) return Err(std::move(*sock_err));

    auto socket = *sock;
    auto hint = *resolve;

    if (args.as_server && ::bind(socket, hint->ai_addr, hint->ai_addrlen) < 0) {
        delameta_detail_close_socket(socket);
        ::freeaddrinfo(hint);
        return Err(log_error(errno, ::strerror));
    }

    info(file, line, "Created UDP socket: " + std::to_string(socket));

    return Ok(UDP(file, line, socket, args.timeout, hint));
}

UDP::UDP(const char* file, int line, int socket, int timeout, void* peer)
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , socket(socket)
    , timeout(timeout)
    , peer(peer)
    , file(file)
    , line(line) { delameta_detail_set_non_blocking(socket); }

UDP::UDP(UDP&& other)
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , socket(std::exchange(other.socket, -1))
    , timeout(other.timeout)
    , peer(std::exchange(other.peer, nullptr))
    , file(other.file)
    , line(other.line) {}

UDP::~UDP() {
    if (socket >= 0) {
        delameta_detail_close_socket(socket);
        info(file, line, "Closed UDP socket: " + std::to_string(socket));
        socket = -1;
    } 
    if (peer) {
        ::freeaddrinfo(reinterpret_cast<struct addrinfo*>(peer));
    }
}

auto UDP::read() -> Result<std::vector<uint8_t>> {
    return delameta_detail_recvfrom(file, line, socket, timeout, peer);
}

auto UDP::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    return delameta_detail_recvfrom_until(file, line, socket, timeout, peer, n);
}

auto UDP::read_as_stream(size_t n) -> Stream {
    return delameta_detail_read_as_stream(file, line, timeout, this, n);
}

auto UDP::write(std::string_view data) -> Result<void> {
    return delameta_detail_sendto(file, line, socket, timeout, peer, data);
}

auto Server<UDP>::start(const char* file, int line, Args args) -> Result<void> {
    auto [udp, err] = UDP::Open(file, line, {args.host, true, args.timeout});
    if (err) return Err(std::move(*err));

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

    while (is_running and delameta_detail_is_socket_alive(udp->socket)) {
        auto sa_in = new ::sockaddr_in();
        auto ai = new ::addrinfo();
        ::memset(sa_in, 0, sizeof(::sockaddr_in));
        ::memset(ai, 0, sizeof(::addrinfo));
        ai->ai_addr = reinterpret_cast<::sockaddr*>(sa_in);
        ai->ai_addrlen = sizeof(sockaddr_in);

        UDP session(file, line, udp->socket, udp->timeout, ai);

        auto read_result = session.read();
        if (read_result.is_err()) {
            delete sa_in;
            delete ai;
            session.socket = -1; // prevent closing the socket
            session.peer = nullptr;

            if (read_result.unwrap_err().code == Error::TransferTimeout) continue;
            else break;
        }

        std::lock_guard<std::mutex> lock(mtx);
        threads.emplace_back([
                this, 
                session=std::move(session), sa_in, ai, 
                data=std::move(read_result.unwrap()), 
                &threads, &mtx, &is_running
        ]() mutable {
            auto stream = execute_stream_session(session, delameta_detail_get_ip(session.socket), data);
            stream >> session;
            session.socket = -1; // prevent closing the socket
            session.peer = nullptr;
            delete sa_in;
            delete ai;

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

void Server<UDP>::stop() {
    if (on_stop) {
        on_stop();
    }
}
