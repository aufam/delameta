#ifndef PROJECT_DELAMETA_URL_H
#define PROJECT_DELAMETA_URL_H

#include <string>
#include <unordered_map>

namespace Project::delameta {
    struct URL {
        URL() = default;
        URL(std::string url);

        std::string url;
        std::string protocol;
        std::string host;
        std::string path;
        std::string full_path;
        std::unordered_map<std::string, std::string> queries;
        std::string fragment;

        static std::string encode(const std::unordered_map<std::string, std::string>&);
        static std::unordered_map<std::string, std::string> decode(std::string_view);
    };
}

#ifdef FMT_FORMAT_H_

template <> 
struct fmt::formatter<Project::delameta::URL> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const Project::delameta::URL& m, Ctx& ctx) const {
        return fmt::format_to(ctx.out(), 
            "\"{}\": {{protocol: \"{}\", host: \"{}\", path: \"{}\", full path: \"{}\", queries: {}, fragment: {}}}", 
            m.url, m.protocol, m.host, m.path, m.full_path, m.queries, m.fragment);
    }
};

#endif
#endif
