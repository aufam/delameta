#include "delameta/http/server.h"
#include "debug.ipp"
#include "on_sigint.ipp"
#include "options.ipp"

using Project::delameta::http::Server;
using TCPServer = Project::delameta::tcp::Server;
using Project::delameta::URL;
using Project::delameta::Error;
using Project::delameta::info;
using Project::delameta::panic;

static Server app;

void example_init(Server& app);
void exec_init(Server& app);
void file_handler_init(Server& app);
void serial_handler_init(Server& app);
void modbus_rtu_init(Server& app);
void larkin_init(Server& app);
void jwt_init(Server& app);

int main(int argc, char* argv[]) {
    static const auto hostname_default = "localhost:5000";
    std::string hostname = hostname_default;

    execute_options(argc, argv, {
        {'H', "host", required_argument, [&](const char* arg) {
            hostname = arg;
        }},
        {'h', "help", no_argument, [](const char*) {
            std::cout << 
                "Delameta API\n"
                "Options:\n"
                "-H, --host Specify the server host. Default = " << hostname_default << "\n"
                "-h, --help Print help\n";
            exit(0);
        }}
    });

    URL host = hostname;
    info(__FILE__, __LINE__, "debug: hostname: " + hostname);
    auto server = TCPServer::New(__FILE__, __LINE__, {hostname}).expect([](Error err) {
        panic(__FILE__, __LINE__, err.what);
    });

    app.bind(server);

    example_init(app);
    exec_init(app);
    file_handler_init(app);
    serial_handler_init(app);
    modbus_rtu_init(app);
    larkin_init(app);
    jwt_init(app);

    on_sigint([&]() { server.stop(); });

    info(__FILE__, __LINE__, "Server is starting on " + host.host);
    server.start().expect([](Error err) {
        panic(__FILE__, __LINE__, err.what);
    });

    return 0;
}
