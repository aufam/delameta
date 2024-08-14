#include "delameta/tcp/client.h"
#include "delameta/debug.h"
#include "etl/time.h"
#include "socket.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

static int client_port = 50000;

auto tcp::Client::New(const char* file, int line, Args args) -> Result<Client> {
    return Socket::New(file, line, Sn_MR_TCP, client_port++, 0).then([&](Socket socket) {
        ::connect(socket.socket, args.ip.data(), args.port);
        return tcp::Client(new Socket(std::move(socket)));
    });
}

tcp::Client::Client(Socket* socket) : StreamSessionClient(socket) {}
