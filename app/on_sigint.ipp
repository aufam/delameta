#include <functional>
#include <csignal>

void on_sigint(std::function<void()> fn) {
    static std::function<void()> at_exit;
    at_exit = std::move(fn);
    auto sig = +[](int) { at_exit(); };
    std::signal(SIGINT, sig);
    std::signal(SIGTERM, sig);
    std::signal(SIGQUIT, sig);
}