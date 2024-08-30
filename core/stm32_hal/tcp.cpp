#ifndef DELAMETA_STM32_DISABLE_SOCKET

#include "main.h"
#include "socket.h"
#include "delameta/tcp.h"
#include "delameta/debug.h"
#include <etl/async.h>
#include <etl/heap.h>
#include <cstring>

extern bool delameta_wizchip_is_setup;

using namespace Project;
using namespace Project::delameta;
using namespace etl::literals;
using etl::Err;
using etl::Ok;

struct socket_descriptor_t {
    bool is_busy;
};
extern socket_descriptor_t delameta_wizchip_socket_descriptors[_WIZCHIP_SOCK_NUM_];

struct addrinfo {
    uint8_t ip[4];
    uint16_t port;
};
auto delameta_detail_resolve_domain(const std::string& domain) -> Result<addrinfo>;
auto delameta_wizchip_socket_open(uint8_t protocol, int port, int flag) -> Result<int>;

int delameta_wizchip_dummy_client_port = 50000;

auto TCP::Open(const char* file, int line, Args args) -> Result<TCP> {
    auto hint = delameta_detail_resolve_domain(args.host);
    if (hint.is_err()) {
        return Err(std::move(hint.unwrap_err()));
    }

    auto port = delameta_wizchip_dummy_client_port++;
    if (delameta_wizchip_dummy_client_port == 0xffff) {
        delameta_wizchip_dummy_client_port = 50000;
    }

    return delameta_wizchip_socket_open(Sn_MR_TCP, port, 0).then([&](int sock) {
        ::connect(sock, hint.unwrap().ip, hint.unwrap().port);
        return TCP(file, line, sock, args.timeout);
    });
}

TCP::TCP(const char* file, int line, int socket, int timeout) 
    : Descriptor()
    , StreamSessionClient(this)
    , socket(socket)
    , keep_alive(true)
    , timeout(timeout)
    , max(-1) 
    , file(file)
    , line(line) {}

TCP::TCP(TCP&& other) 
    : Descriptor()
    , StreamSessionClient(this)
    , socket(std::exchange(other.socket, -1))
    , keep_alive(other.keep_alive)
    , timeout(other.timeout)
    , max(other.max) 
    , file(other.file)
    , line(other.line) {}

TCP::~TCP() {
    if (socket >= 0) {
        ::close(socket);
        delameta_wizchip_socket_descriptors[socket].is_busy = false;
        socket = -1;
    }
}

auto TCP::read() -> Result<std::vector<uint8_t>> {
    auto start = etl::time::now();
    while (true) {
        int stat = getSn_SR(socket);
        if (stat != SOCK_ESTABLISHED) {
            return Err(Error{stat, "Closed by peer"});
        }

        size_t len = ::getSn_RX_RSR(socket);
        if (len == 0) {
            if (timeout > 0 && etl::time::elapsed(start) > etl::time::seconds(timeout)) {
                return Err(Error::TransferTimeout);
            }
            etl::time::sleep(10ms);
            continue;
        }

        if (etl::heap::freeSize < len) {
            return Err(Error{-1, "No memory"});
        }

        auto res = std::vector<uint8_t>(len);
        int32_t sz = ::recv(socket, res.data(), len);
        if (sz <= 0) {
            return Err(Error::ConnectionClosed);
        }
        res.resize(sz);

        return Ok(std::move(res));
    }
}

auto TCP::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    auto start = etl::time::now();
    std::vector<uint8_t> buffer(n);
    size_t remaining_size = n;
    auto ptr = buffer.data();

    while (true) {
        int stat = getSn_SR(socket);
        if (stat != SOCK_ESTABLISHED) {
            return Err(Error{stat, "Closed by peer"});
        }

        size_t len = ::getSn_RX_RSR(socket);
        if (len == 0) {
            if (timeout > 0 && etl::time::elapsed(start) > etl::time::seconds(timeout)) {
                return Err(Error::TransferTimeout);
            }
            etl::time::sleep(10ms);
            continue;
        }

        auto size = std::min(remaining_size, len);
        int32_t sz = ::recv(socket, ptr, size);
        if (sz <= 0) {
            return Err(Error::ConnectionClosed);
        }
        remaining_size -= sz;
        ptr += sz;

        if (remaining_size == 0) {
            return Ok(std::move(buffer));
        }
    }
}

auto TCP::read_as_stream(size_t n) -> Stream {
    Stream s;
    for (int total = n; total > 0;) {
        int size = std::min(total, 2048);
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

auto TCP::write(std::string_view data) -> Result<void> {
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        int stat = getSn_SR(socket);
        if (stat != SOCK_ESTABLISHED) {
            return Err(Error{stat, "Closed by peer"});
        }

        auto n = std::min<size_t>(2048, data.size() - i);
        auto sent = ::send(socket, (uint8_t*)&data[i], n);
        
        if (sent == 0) {
            return Err(Error::ConnectionClosed);
        } else if (sent < 0) {
            return Err(Error{sent, "socket write"});
        }

        total += sent;
        i += sent;
    }

    return Ok();
}

auto Server<TCP>::start(const char* file, int line, Args args) -> Result<void> {
    auto hint = delameta_detail_resolve_domain(args.host);
    if (hint.is_err()) {
        return Err(std::move(hint.unwrap_err()));
    }

    auto port = hint.unwrap().port;

    auto [sock, sock_err] = delameta_wizchip_socket_open(Sn_MR_TCP, port, Sn_MR_ND);
    if (sock_err) return Err(std::move(*sock_err));

    TCP session(file, line, *sock, -1);

    bool is_running {true};
    on_stop = [this, &is_running]() { 
        is_running = false;
        on_stop = {};
    };

    while (is_running) {
        ::listen(session.socket);

        while (getSn_SR(session.socket) != SOCK_ESTABLISHED) {
            etl::time::sleep(1ms);
        }

        for (int cnt = 1; is_running; ++cnt) {
            auto read_result = session.read();
            if (read_result.is_err()) {
                warning(session.file, session.line, read_result.unwrap_err().what);
                break;
            }

            auto stream = execute_stream_session(session, "TCP", read_result.unwrap());
            stream >> session;

            if (not session.keep_alive) {
                if (session.max > 0 and cnt >= session.max) {
                    info(session.file, session.line, "Reached maximum receive: " + std::to_string(session.socket));
                }
                break;
                info(session.file, session.line, "not keep alive");
            }
        }

        ::disconnect(session.socket);
        etl::time::sleep(1ms);
        auto res = ::socket(session.socket, Sn_MR_TCP, port, Sn_MR_ND);
        if (res < 0) {
            warning(session.file, session.line, "Unable to initialize socket again");
            break;
        }
        etl::time::sleep(1ms);
    }

    stop();
    return etl::Ok();
}

void Server<TCP>::stop() {
    if (on_stop) {
        on_stop();
    }
}

#endif