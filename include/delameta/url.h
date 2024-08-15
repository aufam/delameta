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
        std::string host;
        std::string path;
        std::string full_path;
        std::unordered_map<std::string, std::string> queries;
        std::string fragment;
    };
}

#ifdef FMT_FORMAT_H_

template <> 
struct fmt::formatter<Project::delameta::URL> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const Project::delameta::URL& m, Ctx& ctx) const {
        return fmt::format_to(ctx.out(), "{}: {{protocol: {}, host: {}, path: {}, full path: {}, queries: {}, fragment: {}}}", 
            m.url, m.host, m.path, m.full_path, m.queries, m.fragment);
    }
};

#endif
#endif
