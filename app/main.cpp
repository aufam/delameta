#include "delameta/http/server.h"
#include "debug.ipp"
#include "on_sigint.ipp"
#include "options.ipp"

using Project::delameta::http::Server;
using Project::delameta::URL;
using Project::delameta::Error;
using Project::delameta::info;
using Project::delameta::panic;

void example_init(Server& app);
void file_handler_init(Server& app);
void serial_handler_init(Server& app);
void modbus_rtu_init(Server& app);
void larkin_init(Server& app);

int main(int argc, char* argv[]) {
    std::string hostname = "localhost:5000";

    execute_options(argc, argv, {
        {'H', "host", required_argument, [&](const char* arg) {
            hostname = arg;
        }},
        {'h', "help", no_argument, [](const char*) {
            std::cout << "Delameta API\n";
            std::cout << "Options:\n";
            std::cout << "-H, --host        Specify the server host. Default = localhost:5000\n";
            std::cout << "-h, --help        Print help\n";
            exit(0);
        }}
    });

    URL host = hostname;
    info(__FILE__, __LINE__, "debug: hostname: " + hostname);
    auto app = Server::New(__FILE__, __LINE__, {hostname}).expect([](Error err) {
        panic(__FILE__, __LINE__, err.what);
    });

    example_init(app);
    file_handler_init(app);
    serial_handler_init(app);
    modbus_rtu_init(app);
    larkin_init(app);

    on_sigint([&]() { app.stop(); });

    info(__FILE__, __LINE__, "Server is starting on " + host.host);
    app.start().expect([](Error err) {
        panic(__FILE__, __LINE__, err.what);
    });

    return 0;
}
