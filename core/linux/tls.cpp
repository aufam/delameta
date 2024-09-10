#include "delameta/tls.h"
#include "helper.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
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

static std::atomic<int> ssl_counter;
static SSL_CTX* ssl_context_server;
static SSL_CTX* ssl_context_client;

static auto ssl_get_error() -> Error {
    char buf[128];
    auto code = ERR_get_error();
    ERR_error_string_n(code, buf, 128);
    return {int(code), buf};
}

static auto ssl_init() -> Result<void> {
    if (ssl_counter.load() == 0) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();

        auto method_client = TLS_client_method();
        ssl_context_client = SSL_CTX_new(method_client);

        if (!ssl_context_client) {
            return Err(ssl_get_error());
        }

        auto method_server = TLS_server_method();
        ssl_context_server = SSL_CTX_new(method_server);

        if (!ssl_context_server) {
            return Err(ssl_get_error());
        }
    }

    ++ssl_counter;
    return Ok();
}

static void ssl_deinit() {
    --ssl_counter;
    if (ssl_counter.load() == 0) {
        SSL_CTX_free(ssl_context_server);
        SSL_CTX_free(ssl_context_client);
        EVP_cleanup();
        ERR_free_strings();
    }
}

static auto ssl_handshake(int socket, bool as_server) -> Result<SSL*> {
    auto [_, err] = ssl_init();
    if (err) return Err(std::move(*err));

    auto ssl = SSL_new(as_server ? ssl_context_server : ssl_context_client);
    SSL_set_fd(ssl, socket);

    int res;
    while (true) {
        res = as_server ? SSL_accept(ssl) : SSL_connect(ssl);
        int ssl_error = SSL_get_error(ssl, res);
        if (res == 1) 
            break;

        if (ssl_error == SSL_ERROR_WANT_READ) {
            // Wait for the socket to be ready for reading (use select/poll/epoll)
            info(__FILE__, __LINE__, "Waiting for socket to be readable...");
        } else if (ssl_error == SSL_ERROR_WANT_WRITE) {
            // Wait for the socket to be ready for writing
            info(__FILE__, __LINE__, "Waiting for socket to be writable...");
        } else {
            warning(__FILE__, __LINE__, "SSL handshake failed: " + std::to_string(ssl_error));
            break;
        }

        std::this_thread::sleep_for(100ms);
    }
    

    if (res <= 0) return Err(ssl_get_error());

    return Ok(ssl);
}

static auto ssl_context_server_configure(const std::string& cert_file, const std::string& key_file) -> Result<void> {
    auto [_, err] = ssl_init();
    if (err) return Err(std::move(*err));

    if (SSL_CTX_use_certificate_file(ssl_context_server, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        return Err(ssl_get_error());
    }

    if (SSL_CTX_use_PrivateKey_file(ssl_context_server, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        return Err(ssl_get_error());
    }

    return Ok();
}

auto TLS::Open(const char* file, int line, Args args) -> Result<TLS> {
    auto [tcp, tcp_err] = TCP::Open(file, line, std::move(args));
    if (tcp_err) return Err(std::move(*tcp_err));

    int flags = fcntl(tcp->socket, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(tcp->socket, F_SETFL, flags);

    auto [ssl, ssl_err] = ssl_handshake(tcp->socket, false);
    if (ssl_err) return Err(std::move(*ssl_err));

    return Ok(TLS(std::move(*tcp), *ssl));
}

TLS::TLS(TCP&& tcp, void* ssl)
    : TCP(std::move(tcp)) 
    , ssl(ssl) {}

TLS::TLS(const char* file, int line, int socket, int timeout, void* ssl)
    : TCP(file, line, socket, timeout)
    , ssl(ssl) {}

TLS::TLS(TLS&& other)
    : TCP(std::move(other)) 
    , ssl(std::exchange(other.ssl, nullptr)) {}

TLS::~TLS() {
    if (ssl) {
        SSL_free(reinterpret_cast<SSL*>(ssl));
        ssl_deinit();
    }
}

auto TLS::read() -> Result<std::vector<uint8_t>> {
    return delameta_detail_read(file, line, socket, ssl, timeout, delameta_detail_is_socket_alive);
}

auto TLS::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    return delameta_detail_read_until(file, line, socket, ssl, timeout, delameta_detail_is_socket_alive, n);
}

auto TLS::read_as_stream(size_t n) -> Stream {
    return delameta_detail_read_as_stream(file, line, timeout, this, n);
}

auto TLS::write(std::string_view data) -> Result<void> {
    return delameta_detail_write(file, line, socket, ssl, timeout, delameta_detail_is_socket_alive, data);
}

auto Server<TLS>::start(const char* file, int line, Args args) -> Result<void> {
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

    auto [_, conf_err] = ssl_context_server_configure(args.cert_file, args.key_file);
    if (conf_err) return Err(std::move(*conf_err));

    auto defer_conf = defer | &ssl_deinit;

    info(file, line, "Created socket as TLS server: " + std::to_string(socket));

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

            auto [ssl, ssl_err] = ssl_handshake(sock_client, true);
            if (ssl_err) {
                ::close(sock_client);
                continue;
            }

            if (SSL_accept(*ssl) <= 0) {
                ::close(sock_client);
                continue;
            }

            TLS session(file, line, sock_client, 1, *ssl);
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
                SSL_shutdown(*ssl);
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

void Server<TLS>::stop() {
    if (on_stop) {
        on_stop();
    }
}