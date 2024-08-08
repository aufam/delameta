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
#include "../delameta.h"

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
    
        info(file, line, "Created socket server: " + std::to_string(server->socket));
        return Ok(Server(new Socket(std::move(*server))));
    }
    
    return Err(std::move(err));
}

tcp::Server::Server(Socket* socket) 
    : StreamSessionServer({})
    , socket(socket) {}

tcp::Server::Server(Server&& other) 
    : StreamSessionServer(std::move(other.handler))
    , socket(std::exchange(other.socket, nullptr))
    , on_stop(std::move(other.on_stop)) {}

auto tcp::Server::operator=(Server&& other) -> Server& {
    if (this == &other) {
        return *this;
    }

    this->~Server();
    socket = std::exchange(other.socket, nullptr);
    handler = std::move(other.handler);
    on_stop = std::move(other.on_stop);
    return *this;
}

tcp::Server::~Server() {
    stop();
    if (socket) {
        delete socket;
        socket = nullptr;
    }
}

auto tcp::Server::start() -> Result<void> {
    if (socket == nullptr) {
        std::string what = "No server socket created";
        warning(__FILE__, __LINE__, what);
        return Err(Error{-1, what});
    }

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

    while (is_running) {
        auto client = Socket::Accept(__FILE__, __LINE__, socket->socket, nullptr, nullptr);
        if (client.is_err()) {
            Error& err = client.unwrap_err();
            if (err.code == EWOULDBLOCK || err.code == EAGAIN) {
                // no incoming connection, sleep briefly
                std::this_thread::sleep_for(10ms);
            } else {
                warning(__FILE__, __LINE__, err.what);
            }
            continue;
        }

        std::lock_guard<std::mutex> lock(mtx);
        threads.emplace_back([this, client=std::move(client.unwrap()), &threads, &mtx, &is_running]() mutable {
            auto client_ip = delameta_detail_get_ip(client.socket);

            for (int cnt = 1; is_running and delameta_detail_is_socket_alive(client.socket); ++cnt) {
                auto received_result = client.read();
                if (received_result.is_err()) {
                    break;
                }

                auto stream = execute_stream_session(client, client_ip, received_result.unwrap());
                stream >> client;

                if (not client.keep_alive) {
                    if (client.max > 0 and cnt >= client.max) {
                        warning(client.file, client.line, "Reached maximum receive: " + std::to_string(client.socket));
                    }
                    break;
                }
            }

            // shutdown if still connected
            if (delameta_detail_is_socket_alive(client.socket)) {
                ::shutdown(client.socket, SHUT_RDWR);
                info(client.file, client.line, "Closed by server: " + std::to_string(client.socket));
            } else {
                info(client.file, client.line, "Closed by peer: " + std::to_string(client.socket));
            }

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

void tcp::Server::stop() {
    if (on_stop) {
        on_stop();
    }
}