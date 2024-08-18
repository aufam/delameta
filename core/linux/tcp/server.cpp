#include "delameta/tcp/server.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include <etl/string_view.h>
#include "../helper.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

auto delameta_detail_resolve_domain(const std::string& domain, bool for_binding) -> etl::Result<struct addrinfo*, int>;

auto tcp::Server::New(const char* file, int line, Args args) -> Result<Server> {
    auto log_error = [file, line](int code, auto err_to_str) {
        std::string what = err_to_str(code);
        warning(file, line, what);
        return Error{code, what};
    };

    auto [resolve, resolve_err] = delameta_detail_resolve_domain(args.host, true);
    if (resolve_err) return Err(log_error(*resolve_err, ::gai_strerror));

    auto hint = *resolve;
    auto se = defer | [&]() { ::freeaddrinfo(hint); };
    Error err{-1, "Unable to resolve hostname: " + args.host};

    for (auto p = hint; p != nullptr; p = p->ai_next) {
        auto [server, server_err] = Socket::New(file, line, p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_err) {
            err = std::move(*server_err);
            continue;
        }

        if (int enable = 1; ::setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            err = log_error(errno, ::strerror);
            continue;
        }

        if (::bind(server->socket, p->ai_addr, p->ai_addrlen) < 0) {
            err = log_error(errno, ::strerror);
            continue;
        }

        if (::listen(server->socket, args.max_socket) < 0) {
            err = log_error(errno, ::strerror);
            continue;
        }
    
        int port = 0;
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
            port = ntohs(addr->sin_port);
        } else if (p->ai_family == AF_INET6) { // IPv6
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)p->ai_addr;
            port = ntohs(addr->sin6_port);
        }
    
        info(file, line, "Created socket server: " + std::to_string(server->socket));
        return Ok(Server(std::move(*server), port, args.max_socket));
    }
    
    return Err(std::move(err));
}

tcp::Server::Server(Socket&& socket, int port, int max_socket) 
    : StreamSessionServer({})
    , socket(std::move(socket))
    , port(port) 
    , max_socket(max_socket) {}

tcp::Server::Server(Server&& other) 
    : StreamSessionServer(std::move(other.handler))
    , socket(std::move(other.socket))
    , port(other.port)
    , max_socket(other.max_socket)
    , on_stop(std::move(other.on_stop)) {}

tcp::Server::~Server() {}

static void work(tcp::Server* self, std::atomic_bool* is_running, int idx) {
    info(self->socket.file, self->socket.line, "Spawned worker thread: " + std::to_string(idx));
    while (*is_running) {
        auto [client, client_err] = Socket::Accept(__FILE__, __LINE__, self->socket.socket, nullptr, nullptr);
        if (client_err) {
            if (client_err->code == EWOULDBLOCK || client_err->code == EAGAIN) {
                // no incoming connection, sleep briefly
                std::this_thread::sleep_for(10ms);
            } else {
                warning(__FILE__, __LINE__, client_err->what);
            }
            continue;
        }

        client->keep_alive = self->socket.keep_alive;
        client->timeout = self->socket.timeout;
        client->max = self->socket.max;
    
        auto client_ip = delameta_detail_get_ip(client->socket);

        for (int cnt = 1; is_running and delameta_detail_is_socket_alive(client->socket); ++cnt) {
            auto received_result = client->read();
            if (received_result.is_err()) {
                break;
            }

            auto stream = self->execute_stream_session(*client, client_ip, received_result.unwrap());
            stream >> *client;

            if (not client->keep_alive) {
                if (client->max > 0 and cnt >= client->max) {
                    warning(client->file, client->line, "Reached maximum receive: " + std::to_string(client->socket));
                }
                break;
            }
        }

        // shutdown if still connected
        if (delameta_detail_is_socket_alive(client->socket)) {
            ::shutdown(client->socket, SHUT_RDWR);
            info(client->file, client->line, "Closed by server: " + std::to_string(client->socket));
        } else {
            info(client->file, client->line, "Closed by peer: " + std::to_string(client->socket));
        }
    }
}

auto tcp::Server::start() -> Result<void> {
    if (max_socket <= 0) {
        panic(socket.file, socket.line, "max socket must be a positive integer");
    }
    if (max_socket > 8) {
        panic(socket.file, socket.line, "too many socket");
    }

    std::atomic_bool is_running {true};
    std::vector<std::thread> threads;
    threads.reserve(max_socket - 1);

    on_stop = [this, &is_running]() { 
        is_running = false;
        on_stop = {};
    };

    int i = 0;
    for (; i < max_socket - 1; ++i) {
        threads.emplace_back(work, this, &is_running, i);
    }

    work(this, &is_running, i);
    for (auto& thd : threads) if (thd.joinable()) {
        thd.join();
    }
    return etl::Ok();
}

void tcp::Server::stop() {
    if (on_stop) {
        on_stop();
    }
}