#include "delameta/tcp/client.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstring>
#include "../helper.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

auto delameta_detail_resolve_domain(const std::string& domain, bool for_binding) -> etl::Result<struct addrinfo*, int>;

auto tcp::Client::New(const char* file, int line, Args args) -> Result<Client> {
    auto log_error = [file, line](int code, auto err_to_str) {
        std::string what = err_to_str(code);
        warning(file, line, what);
        return Error{code, what};
    };

    auto [resolve, resolve_err] = delameta_detail_resolve_domain(args.host, false);
    if (resolve_err) return Err(log_error(*resolve_err, ::gai_strerror));

    auto hint = *resolve;
    auto se = defer | [&]() { ::freeaddrinfo(hint); };
    Error err{-1, "Unable to resolve hostname: " + args.host};

    for (auto p = hint; p != nullptr; p = p->ai_next) {
        auto [client, client_err] = Socket::New(file, line, p->ai_family, p->ai_socktype, p->ai_protocol);
        if (client_err) {
            err = std::move(*client_err);
            continue;
        }
        
        if (::connect(client->socket, p->ai_addr, p->ai_addrlen) != 0) {
            if (errno != EINPROGRESS) {
                err = log_error(errno, ::strerror);
                continue;
            }

            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(client->socket, &write_fds);

            struct timeval tv;
            tv.tv_sec = args.timeout;
            tv.tv_usec = 0;

            if (::select(client->socket + 1, nullptr, &write_fds, nullptr, &tv) <= 0) {
                err = log_error(errno, ::strerror);
                continue;
            }

            // Connection established or error occurred
            int error_code = 0;
            socklen_t len = sizeof(error_code);
            if (::getsockopt(client->socket, SOL_SOCKET, SO_ERROR, &error_code, &len) != 0 || error_code != 0){
                err = log_error(errno, ::strerror);
                continue;
            }
        } 

        info(file, line, "Created socket client: " + std::to_string(client->socket));
        return Ok(Client(new Socket(std::move(*client))));
    }

    return Err(std::move(err));
}

tcp::Client::Client(Socket* socket) : StreamSessionClient(socket) {}
