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
        Error(const char* what);
        virtual ~Error() = default;
    
        int code;
        std::string what;
        operator const char*() const { return what.c_str(); }
    };

    template <typename T>
    using Result = etl::Result<T, Error>;
}

namespace Project::delameta::detail {
    // unwrap helper for macro TRY
    template <typename T, typename E> 
    auto unwrap(etl::Result<T, E>& res) {
        if constexpr (std::is_void_v<T>) {
            return nullptr; // no need to unwrap, but still need to return some value
        } else {
            return std::move(res.unwrap());
        }
    }
}

// try to unwrap the result of an expression, or return the error to the scope.
// you may need to disable the `-pedantic` flag
#define TRY(expr) ({ \
    auto try_res = (expr); \
    if (try_res.is_err()) return ::Project::etl::Err(std::move(try_res.unwrap_err())); \
    ::Project::delameta::detail::unwrap(try_res); \
})

// try to unwrap the result of an expression, or do expr2.
// you may need to disable the `-pedantic` flag
#define TRY_OR(expr, expr2) ({ \
    auto try_res = (expr); \
    if (try_res.is_err()) expr2; \
    ::Project::delameta::detail::unwrap(try_res); \
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