#include "delameta/tcp/client.h"
#include "delameta/debug.h"
#include "etl/string_view.h"
#include "etl/time.h"
#include <socket.h>

using namespace Project;
using namespace Project::delameta;

using etl::Err;
using etl::Ok;

static int client_port = 50000;

struct addrinfo {
    uint8_t ip[4];
    int port;
};

auto delameta_detail_resolve_domain(const std::string& domain) -> Result<addrinfo>;

auto tcp::Client::New(const char* file, int line, Args args) -> Result<Client> {
    auto ip = delameta_detail_resolve_domain(args.host);
    if (ip.is_err()) {
        return Err(std::move(ip.unwrap_err()));
    }
    return Socket::New(file, line, Sn_MR_TCP, client_port++, 0).and_then([&](Socket socket) -> Result<Client> {
        ::connect(socket.socket, ip.unwrap().ip, ip.unwrap().port);
        return Ok(tcp::Client(new Socket(std::move(socket))));
    });
}

tcp::Client::Client(Socket* socket) : StreamSessionClient(socket) {}
