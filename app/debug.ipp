#include <iostream>
#include <iomanip>
#include <chrono>

namespace Project::delameta {
    inline auto debug_time() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm* local_time = std::localtime(&now_c);
        return std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
    }

    inline void info(const char* file, int line, const std::string& msg) {
        std::cout << debug_time() << " " << file << ":" << std::to_string(line) << ": INFO: " << msg << '\n';
    }
    inline void warning(const char* file, int line, const std::string& msg) {
        std::cout << debug_time() << " " << file << ":" << std::to_string(line) << ": WARNING: " << msg << '\n';
    }
    inline void panic(const char* file, int line, const std::string& msg) {
        std::cout << debug_time() << " " << file << ":" << std::to_string(line) << ": PANIC: " << msg << '\n';
        exit(1);
    }
}