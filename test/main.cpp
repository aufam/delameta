#include "gtest/gtest.h"
#include <iostream>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    std::cout << (res == 0 ? "SUCCESS\n" : "FAILED\n");
    return res;
}

// mingw cannot see weak functions
#ifdef _WIN32
#include <delameta/debug.h>

namespace Project::delameta {
    void info(const char*, int, const std::string&) {}

    void warning(const char*, int, const std::string&) {}

    void panic(const char*, int, const std::string& msg) {
        std::cerr << msg << '\n';
        exit(1);
    }
}
#endif
