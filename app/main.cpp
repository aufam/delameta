#include "delameta/http/server.h"
#include "debug.ipp"
#include "on_sigint.ipp"

using Project::delameta::http::Server;
using Project::delameta::Error;
using Project::delameta::panic;

void example_init(Server& app);
void file_handler_init(Server& app);
void serial_handler_init(Server& app);
void modbus_rtu_init(Server& app);
void larkin_init(Server& app);

int main() {
    auto app = Server::New(__FILE__, __LINE__, {"0.0.0.0", 5000}).expect([](Error err) {
        panic(__FILE__, __LINE__, err.what);
    });

    example_init(app);
    file_handler_init(app);
    serial_handler_init(app);
    modbus_rtu_init(app);
    larkin_init(app);

    on_sigint([&]() { app.stop(); });

    app.start().expect([](Error err) {
        panic(__FILE__, __LINE__, err.what);
    });

    return 0;
}
