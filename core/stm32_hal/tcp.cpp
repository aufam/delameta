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
using etl::defer;

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
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , socket(socket)
    , keep_alive(true)
    , timeout(timeout)
    , max(-1) 
    , file(file)
    , line(line) {}

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

auto TCP::write(std::string_view data) -> Result<void> {
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        int stat = getSn_SR(socket);
        if (stat != SOCK_ESTABLISHED) {
            return Err(Error{stat, "Closed by peer"});
        }

        auto n = std::min<size_t>(2048, data.size() - i);
        auto sent = ::send(socket, (uint8_t*)&data[i], n);
        etl::time::sleep(1ms); // TODO: blocking mode is not really blocking (?)

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

    if (args.max_socket >= etl::task::resources()) return Err(Error{-1, "no tasks"});
    if (args.max_socket <= 0) return Err(Error{-1, "invalid arg"});

    std::vector<TCP*> sessions;
    sessions.reserve(args.max_socket - 1);

    for (int i = 0; i < args.max_socket - 1; ++i) {
        auto [sock, sock_err] = delameta_wizchip_socket_open(Sn_MR_TCP, port, Sn_MR_ND);
        if (sock_err) return Err(std::move(*sock_err));

        sessions.push_back(new TCP(file, line, *sock, 1));
    }

    auto [sock, sock_err] = delameta_wizchip_socket_open(Sn_MR_TCP, port, Sn_MR_ND);
    if (sock_err) return Err(std::move(*sock_err));
    TCP main_session(file, line, *sock, 1);

    bool is_running {true};
    on_stop = [this, &is_running]() { 
        is_running = false;
        on_stop = {};
    };

    auto work = +[](
        StreamSessionServer* self,
        TCP* session, 
        int port,
        int* session_sem, 
        bool* is_running,
        std::vector<TCP*>* sessions,
        osThreadId_t join_thd
    ) {
        while (*is_running) {
            ::listen(session->socket);
            while (getSn_SR(session->socket) != SOCK_ESTABLISHED) {
                etl::time::sleep(1ms);
            }

            for (int cnt = 1; *is_running; ++cnt) {
                auto read_result = session->read();
                if (read_result.is_err()) {
                    warning(session->file, session->line, read_result.unwrap_err().what);
                    break;
                }

                auto stream = self->execute_stream_session(*session, "TCP", read_result.unwrap());
                stream >> *session;

                if (not session->keep_alive) {
                    if (session->max > 0 and cnt >= session->max) {
                        info(session->file, session->line, "Reached maximum receive: " + std::to_string(session->socket));
                    }
                    break;
                }
            }

            ::disconnect(session->socket);
            etl::time::sleep(1ms);
            ::close(session->socket);
            etl::time::sleep(1ms);
            auto res = ::socket(session->socket, Sn_MR_TCP, port, Sn_MR_ND);
            if (res < 0) {
                warning(session->file, session->line, "Unable to initialize socket again");
                break;
            }
            etl::time::sleep(1ms);
        }

        if (session_sem != nullptr && ++(*session_sem) == (int)sessions->size()) {
            // notify main session
            osThreadFlagsSet(join_thd, 0b1);
        }
    };

    int sem = 0;
    auto join_thd = osThreadGetId();

    for (auto session: sessions) {
        // launch each session in a separate task
        etl::async(std::move(work), this, session, port, &sem, &is_running, &sessions, join_thd);
    }

    work(this, &main_session, port, nullptr, &is_running, &sessions, join_thd);
    if (sessions.size() > 0) {
        // join all session threads;
        osThreadFlagsWait(0b1, osFlagsWaitAny, osWaitForever);
    }

    stop();
    for (auto session : sessions) {
        delete session;
    }

    return etl::Ok();
}

void Server<TCP>::stop() {
    if (on_stop) {
        on_stop();
    }
}

#else
#include "delameta/tcp.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;

#define NOT_IMPLEMENTED return Err(Error{-1, "no impl"});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

auto TCP::Open(const char* file, int line, Args args) -> Result<TCP> {
    NOT_IMPLEMENTED
}

TCP::TCP(const char* file, int line, int socket, int timeout) 
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , socket(socket)
    , keep_alive(true)
    , timeout(timeout)
    , max(-1) 
    , file(file)
    , line(line) {}

TCP::TCP(TCP&& other) 
    : Descriptor()
    , StreamSessionClient(static_cast<Descriptor&>(*this))
    , socket(std::exchange(other.socket, -1))
    , keep_alive(other.keep_alive)
    , timeout(other.timeout)
    , max(other.max) 
    , file(other.file)
    , line(other.line) {}

TCP::~TCP() {}

auto TCP::read() -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto TCP::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto TCP::read_as_stream(size_t n) -> Stream {
    return {};
}

auto TCP::write(std::string_view data) -> Result<void> {
    NOT_IMPLEMENTED
}

#pragma GCC diagnostic pop
#endif
