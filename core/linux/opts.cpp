extern "C" {
    #include <getopt.h>
}
#include "delameta/opts.h"
#include <iostream>
#include <vector>
#include <algorithm>

using namespace Project;
using namespace delameta;
using etl::Err;
using etl::Ok;

auto Opts::collect_arg_values(
    const char* name, 
    const char* description,
    int argc, char** argv, 
    std::initializer_list<OptionFlags>&& flags
) -> std::unordered_map<char, std::string> {
    std::vector<struct option> long_options;
    std::unordered_map<int, std::function<void(const char*)>> map_functions;

    std::string short_opts;

    std::string help = name;
    help += ": ";
    help += description;
    help += '\n';
    help += "Options:\n";

    for (auto &flag: flags) {
        if (flag.short_ == '\0') continue;

        help += '-';
        help += flag.short_;
        help += ", --";
        help += flag.long_;
        help += "  ";
        help += flag.description;

        short_opts += flag.short_;
        if (flag.kind == required_argument) {
            short_opts += ':';
        } else if (flag.kind == optional_argument) {
            short_opts += "::";
            help += ". Default: ";
            help += flag.default_str;
        }
        help += '\n';
        long_options.push_back({flag.long_, flag.kind, nullptr, flag.short_});
    }
    short_opts += 'h';
    help += "-h, --help  print help\n";

    long_options.push_back({"help", no_argument, nullptr, 'h'});
    long_options.push_back({nullptr, 0, nullptr, 0});

    int option;
    int option_index = 0;
    std::unordered_map<char, std::string> result;
    while ((option = getopt_long(argc, argv, short_opts.c_str(), long_options.data(), &option_index)) != -1) {
        if (option == '?') {
            std::cout << help;
            exit(1);
        }

        if (option == 'h') {
            std::cout << help;
            exit(0);
        }

        result[option] = (optarg != nullptr) ? optarg : "";
    }

    return result;
}

int Opts::handle_error(const Error& err) {
    std::cerr << "Err: " << err.what << '\n';
    return err.code;
}

void Opts::print_result(const char* ptr, int len) {
    if (ptr && len > 0)
        std::cout << "Ok: " << std::string_view(ptr, len) << '\n';
    else if (ptr)
        std::cout << "Ok: " << ptr << '\n';
    else 
        std::cout << "Ok\n";
}