#include <boost/preprocessor.hpp>
#include "delameta/http/server.h"
#include "delameta/tcp/server.h"
#include "delameta/debug.h"
#include "delameta/opts.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <csignal>

using namespace Project;
using namespace Project::delameta;
using http::Server;
using TCPServer = tcp::Server;
using etl::Err;
using etl::Ok;

HTTP_DEFINE_OBJECT(app);

static void on_sigint(std::function<void()> fn);

OPTS_MAIN(
    (ExampleAPI, "Example API"),
    (URL, host, 'H', "host", "Specify host server", "localhost:5000"),
    (Result<void>)
) {
    auto tcp_server = TCPServer::New(FL, {host.host}).expect([](Error err) {
        DBG(panic, err.what);
    });

    app.bind(tcp_server);
    on_sigint([&]() { tcp_server.stop(); });

    DBG(info, "Server is starting on " + host.host);
    return tcp_server.start();
}

static void on_sigint(std::function<void()> fn) {
    static std::function<void()> at_exit;
    at_exit = std::move(fn);
    auto sig = +[](int) { at_exit(); };
    std::signal(SIGINT, sig);
    std::signal(SIGTERM, sig);
    std::signal(SIGQUIT, sig);
}

static auto debug_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&now_c);
    return std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
}

namespace Project::delameta {

    void info(const char* file, int line, const std::string& msg) {
        std::cout << debug_time() << " " << file << ":" << std::to_string(line) << ": INFO: " << msg << '\n';
    }
    void warning(const char* file, int line, const std::string& msg) {
        std::cout << debug_time() << " " << file << ":" << std::to_string(line) << ": WARNING: " << msg << '\n';
    }
    void panic(const char* file, int line, const std::string& msg) {
        std::cout << debug_time() << " " << file << ":" << std::to_string(line) << ": PANIC: " << msg << '\n';
        exit(1);
    }
}
