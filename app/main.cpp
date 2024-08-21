#include <boost/preprocessor.hpp>
#include <fmt/chrono.h>
#include <delameta/debug.h>
#include <delameta/http/server.h>
#include <delameta/tcp/server.h>
#include <delameta/opts.h>
#include <csignal>

using namespace Project;
using namespace Project::delameta;
using etl::Err;
using etl::Ok;

HTTP_DEFINE_OBJECT(app);

static void on_sigint(std::function<void()> fn);

OPTS_MAIN(
    (ExampleAPI, "Example API"),
    (URL, host, 'H', "host", "Specify host server", "localhost:5000"),
    (Result<void>)
) {
    auto tcp_server = TRY(
        tcp::Server::New(FL, {
            .host=host.host,
            .max_socket=4,
        })
    );

    tcp_server.socket.keep_alive = false;

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

static auto format_time_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    return fmt::localtime(time_now);
}

namespace Project::delameta {

    void info(const char* file, int line, const std::string& msg) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} INFO: {}", format_time_now(), file, line, msg);
    }
    void warning(const char* file, int line, const std::string& msg) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} WARNING: {}", format_time_now(), file, line, msg);
    }
    void panic(const char* file, int line, const std::string& msg) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} PANIC: {}", format_time_now(), file, line, msg);
        exit(1);
    }
}
