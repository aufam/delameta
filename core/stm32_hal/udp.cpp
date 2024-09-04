#ifndef DELAMETA_STM32_DISABLE_SOCKET

#include "main.h"
#include "socket.h"
#include "delameta/udp.h"
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

extern int delameta_wizchip_dummy_client_port;
auto delameta_wizchip_socket_open(uint8_t protocol, int port, int flag) -> Result<int>;

auto UDP::Open(const char* file, int line, Args args) -> Result<UDP> {
    auto hint = delameta_detail_resolve_domain(args.host);
    if (hint.is_err()) {
        return Err(std::move(hint.unwrap_err()));
    }

    auto peer = args.as_server ? new addrinfo{} : new addrinfo(hint.unwrap());
    auto port = args.as_server ? hint.unwrap().port : delameta_wizchip_dummy_client_port++;
    if (delameta_wizchip_dummy_client_port == 0xffff) {
        delameta_wizchip_dummy_client_port = 50000;
    }

    return delameta_wizchip_socket_open(Sn_MR_UDP, port, 0).then([&](int sock) {
        return UDP(file, line, sock, args.timeout, peer);
    });
}

UDP::UDP(const char* file, int line, int socket, int timeout, void* peer)
    : Descriptor()
    , StreamSessionClient(this)
    , socket(socket)
    , timeout(timeout)
    , peer(peer)
    , file(file)
    , line(line) {}

UDP::UDP(UDP&& other)
    : Descriptor()
    , StreamSessionClient(this)
    , socket(std::exchange(other.socket, -1))
    , timeout(other.timeout)
    , peer(std::exchange(other.peer, nullptr))
    , file(other.file)
    , line(other.line) {}

UDP::~UDP() {
    if (socket >= 0) {
        ::close(socket);
        delameta_wizchip_socket_descriptors[socket].is_busy = false;
        socket = -1;
        delete reinterpret_cast<addrinfo*>(peer);
    }
}

auto UDP::read() -> Result<std::vector<uint8_t>> {
    auto start = etl::time::now();
    while (true) {
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
        auto peer_ = reinterpret_cast<addrinfo*>(peer);

        int32_t sz = ::recvfrom(socket, res.data(), len, peer_->ip, &peer_->port);
        res.resize(sz);

        return Ok(std::move(res));
    }
}

auto UDP::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    auto start = etl::time::now();
    std::vector<uint8_t> buffer(n);
    size_t remaining_size = n;
    auto ptr = buffer.data();

    while (true) {
        size_t len = ::getSn_RX_RSR(socket);
        if (len == 0) {
            if (timeout > 0 && etl::time::elapsed(start) > etl::time::seconds(timeout)) {
                return Err(Error::TransferTimeout);
            }
            etl::time::sleep(10ms);
            continue;
        }

        auto size = std::min(remaining_size, len);
        auto peer_ = reinterpret_cast<addrinfo*>(peer);

        int32_t sz = ::recvfrom(socket, ptr, size, peer_->ip, &peer_->port);
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

auto UDP::read_as_stream(size_t n) -> Stream {
    Stream s;

    s << [this, total=n, buffer=std::vector<uint8_t>{}](Stream& s) mutable -> std::string_view {
        buffer = {};
        size_t n = std::min(total, (size_t)128);
        auto data = this->read_until(n);

        if (data.is_ok()) {
            buffer = std::move(data.unwrap());
            total -= n;
            s.again = total > 0;
        } else {
            warning(file, line, data.unwrap_err().what);
        }

        return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
    };

    return s;
}

auto UDP::write(std::string_view data) -> Result<void> {
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        auto n = std::min<size_t>(2048, data.size() - i);

        auto peer_ = reinterpret_cast<addrinfo*>(peer);
        auto sent = ::sendto(socket, (uint8_t*)&data[i], n, peer_->ip, peer_->port);
        
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

auto Server<UDP>::start(const char* file, int line, Args args) -> Result<void> {
    auto [udp, err] = UDP::Open(file, line, {args.host, true, args.timeout});

    bool is_running {true};
    on_stop = [this, &is_running]() { 
        is_running = false;
        on_stop = {};
    };

    auto &session = *udp;
    while (is_running) {
        auto read_result = session.read();
        if (read_result.is_err()) {
            break; 
        }

        auto stream = execute_stream_session(session, "UDP", read_result.unwrap());
        stream >> session;
    }

    stop();
    return etl::Ok();
}

void Server<UDP>::stop() {
    if (on_stop) {
        on_stop();
    }
}

#else
#include "delameta/udp.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;

#define NOT_IMPLEMENTED return Err(Error{-1, "no impl"});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

auto UDP::Open(const char* file, int line, Args args) -> Result<UDP> {
    NOT_IMPLEMENTED
}

UDP::UDP(const char* file, int line, int socket, int timeout, void* peer)
    : Descriptor()
    , StreamSessionClient(this)
    , socket(socket)
    , timeout(timeout)
    , peer(peer)
    , file(file)
    , line(line) {}

UDP::UDP(UDP&& other)
    : Descriptor()
    , StreamSessionClient(this)
    , socket(std::exchange(other.socket, -1))
    , timeout(other.timeout)
    , peer(std::exchange(other.peer, nullptr))
    , file(other.file)
    , line(other.line) {}

UDP::~UDP() {}

auto UDP::read() -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto UDP::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto UDP::read_as_stream(size_t n) -> Stream {
    return {};
}

auto UDP::write(std::string_view data) -> Result<void> {
    NOT_IMPLEMENTED
}

#pragma GCC diagnostic pop
#endif