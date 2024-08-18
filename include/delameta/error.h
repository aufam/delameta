#ifndef PROJECT_DELAMETA_ERROR_H
#define PROJECT_DELAMETA_ERROR_H

#include <string>
#include <etl/result.h>

namespace Project::delameta {

    class Error {
    public:
        enum Code {
            ConnectionClosed,
            TransferTimeout,
        };

        Error(Code code);
        Error(int code, std::string what);
        virtual ~Error() = default;
    
        int code;
        std::string what;
        operator const char*() const { return what.c_str(); }
    };

    template <typename T>
    using Result = etl::Result<T, Error>;
}

// try to unwrap the result of an expression, or return the error to the scope.
// you may need to disable the `-pedantic` flag
#define TRY(expr) ({ \
    auto res = (expr); \
    if (res.is_err()) return Err(std::move(res.unwrap_err())); \
    std::move(res.unwrap()); \
})

#ifdef FMT_FORMAT_H_

template <> 
struct fmt::formatter<Project::delameta::Error> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const Project::delameta::Error& m, Ctx& ctx) const {
        return fmt::format_to(ctx.out(), "Error {{code: {}, what: {}}}", m.code, m.what);
    }
};

#endif

#endif