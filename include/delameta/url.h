#ifndef PROJECT_DELAMETA_URL_H
#define PROJECT_DELAMETA_URL_H

#include <string>
#include <unordered_map>

namespace Project::delameta {
    struct URL {
        URL() {}
        URL(std::string url);

        std::string url;
        std::string protocol;
        std::string ip;
        int port = {};
        std::string host;
        std::string path;
        std::string full_path;
        std::unordered_map<std::string, std::string> queries;
        std::string fragment;
    };
}

#endif
