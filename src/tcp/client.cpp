#include "delameta/tcp/client.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include "../delameta.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;

static auto log_errno(const char* file, int line) {
    int code = errno;
    std::string what = ::strerror(code);
    warning(file, line, what);
    return Err(Error{code, what});
};

auto tcp::Client::New(const char* file, int line, Args args) -> Result<Client> {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = ::htons(args.port);
    if (::inet_pton(AF_INET, args.host.c_str(), &server_addr.sin_addr) <= 0)
        return log_errno(file, line);

    auto [client, err] = Socket::New(file, line, AF_INET, SOCK_STREAM, 0);
    if (err) 
        return Err(std::move(*err));
    
    if (::connect(client->socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        if (errno != EINPROGRESS)
            return log_errno(file, line);

        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(client->socket, &write_fds);

        struct timeval tv;
        tv.tv_sec = args.timeout;
        tv.tv_usec = 0;

        if (::select(client->socket + 1, nullptr, &write_fds, nullptr, &tv) <= 0)
            return log_errno(file, line);

        // Connection established or error occurred
        int error = 0;
        socklen_t len = sizeof(error);
        if (::getsockopt(client->socket, SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0)
            return log_errno(file, line);
    } 

    info(file, line, "Created socket client: " + std::to_string(client->socket));
    return Ok(Client(new Socket(std::move(*client))));
}

tcp::Client::Client(Socket* socket) : socket(socket) {}

tcp::Client::Client(Client&& other) : socket(std::exchange(other.socket, nullptr)) {}

auto tcp::Client::operator=(Client&& other) -> Client& {
    if (this == &other) {
        return *this;
    }

    this->~Client();
    socket = std::exchange(other.socket, nullptr);
    return *this;
}

tcp::Client::~Client() {
    if (socket) {
        delete socket;
        socket = nullptr;
    }
}

auto tcp::Client::request(Stream& in_stream) -> Result<std::vector<uint8_t>> {
    if (socket == nullptr) {
        std::string what = "No client socket created";
        warning(__FILE__, __LINE__, what);
        return Err(Error{-1, what});
    }

    in_stream >> *socket;
    return socket->read();
}
