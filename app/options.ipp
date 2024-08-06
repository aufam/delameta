extern "C" {
    #include <getopt.h>
}
#include <functional>
#include <initializer_list>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

struct LongOption {
    char short_;
    const char* long_;
    int arg;
    std::function<void(const char*)> fn;
};

static void execute_options(int argc, char* argv[], std::initializer_list<LongOption>&& long_option_list) {
    std::vector<struct option> long_options;
    std::unordered_map<int, std::function<void(const char*)>> map_functions;
    std::string short_opts;

    for (auto &long_option: long_option_list) {
        map_functions[long_option.short_] = std::move(long_option.fn);
        short_opts += long_option.short_;
        if (long_option.arg != no_argument) {
            short_opts += ':';
        }
        long_options.push_back({long_option.long_, long_option.arg, nullptr, long_option.short_});
    }
    long_options.push_back({nullptr, 0, nullptr, 0});
    map_functions['?'] = [] (const char*) { exit(1); };

    int option;
    int option_index = 0;
    while ((option = getopt_long(argc, argv, short_opts.c_str(), long_options.data(), &option_index)) != -1) {
        const char* argument = (optarg != nullptr) ? optarg : "";
        map_functions.at(option)(argument);
    }
}