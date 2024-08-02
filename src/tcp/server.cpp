#include "delameta/tcp/server.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include "../delameta.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;

auto tcp::Server::New(const char* file, int line, Args args) -> Result<Server> {
    auto log_errno = [file, line]() {
        int code = errno;
        std::string what = ::strerror(code);
        warning(file, line, what);
        return Err(Error{code, what});
    };

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = ::htons(args.port);
    if (::inet_pton(AF_INET, args.host.c_str(), &server_addr.sin_addr) <= 0)
        return log_errno();
    
    auto [server, err] = socket::Stream::New(file, line, AF_INET, SOCK_STREAM, 0);
    if (err) return Err(std::move(*err));

    if (int enable = 1; ::setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        return log_errno();

    if (::bind(server->socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        return log_errno();

    if (::listen(server->socket, args.max_socket) < 0)
        return log_errno();
    
    info(file, line, "Created socket server: " + std::to_string(server->socket));
    return Ok(Server(new socket::Stream(std::move(*server))));
}

tcp::Server::Server(socket::Stream* stream) : stream(stream) {}

tcp::Server::Server(Server&& other) 
    : stream(std::exchange(other.stream, nullptr))
    , handler(std::move(other.handler))
    , on_stop(std::move(other.on_stop)) {}

auto tcp::Server::operator=(Server&& other) -> Server& {
    if (this == &other) {
        return *this;
    }

    this->~Server();
    stream = std::exchange(other.stream, nullptr);
    handler = std::move(other.handler);
    on_stop = std::move(other.on_stop);
    return *this;
}

tcp::Server::~Server() {
    stop();
    if (stream) {
        delete stream;
        stream = nullptr;
    }
}

auto tcp::Server::start() -> Result<void> {
    if (stream == nullptr) {
        std::string what = "No server stream created";
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
        auto client = socket::Stream::Accept(__FILE__, __LINE__, stream->socket, nullptr, nullptr);
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
                auto received_result = client.receive();
                if (received_result.is_err()) {
                    break;
                }

                execute_stream_session(client, client_ip, received_result.unwrap());
                client.send();

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

void tcp::Server::execute_stream_session(socket::Stream& stream, const std::string& client_ip, const std::vector<uint8_t>& data) {
    if (handler) {
        handler(stream, client_ip, data);
    }
}