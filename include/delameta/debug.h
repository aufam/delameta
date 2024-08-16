#ifndef PROJECT_DELAMETA_DEBUG_H
#define PROJECT_DELAMETA_DEBUG_H

#include <string>

namespace Project::delameta {

    __attribute__((weak)) 
    void info(const char* file, int line, const std::string& msg);

    __attribute__((weak)) 
    void warning(const char* file, int line, const std::string& msg);

    __attribute__((weak)) 
    void panic(const char* file, int line, const std::string& msg);
}

#define FL __FILE__, __LINE__
#define DBG(fn, ...) fn(__FILE__, __LINE__, __VA_ARGS__)

#ifdef FMT_FORMAT_H_
#define DBG_VAL(fn, value) \
    [&]() -> decltype(auto) {\
        auto&& _value = value; \
        fn(__FILE__, __LINE__, fmt::format("{} = {}", #value, _value)); \
        return std::forward<decltype(_value)>(_value); \
    }()
#else

#define DBG_VAL(fn, value) \
    [&]() -> decltype(auto) {\
        auto&& _value = value; \
        fn(__FILE__, __LINE__, #value + std::string(" = ") + std::to_string(_value)); \
        return std::forward<decltype(_value)>(_value); \
    }()
#endif

#if defined(FMT_FORMAT_H_) && defined(BOOST_PREPROCESSOR_HPP)

#define FMT_HELPER_WRAP_SEQUENCE_X(...) ((__VA_ARGS__)) FMT_HELPER_WRAP_SEQUENCE_Y
#define FMT_HELPER_WRAP_SEQUENCE_Y(...) ((__VA_ARGS__)) FMT_HELPER_WRAP_SEQUENCE_X
#define FMT_HELPER_WRAP_SEQUENCE_X0
#define FMT_HELPER_WRAP_SEQUENCE_Y0

#define FMT_HELPER_DEFINE_MEMBER(r, data, elem) \
    BOOST_PP_TUPLE_ELEM(2, 0, elem) BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define FMT_HELPER_STRINGIZE_MEMBER(r, data, elem) \
    BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem)) ": {}, "

#define FMT_HELPER_MEMBER_AS_ARG(r, data, elem) \
    ,m.BOOST_PP_TUPLE_ELEM(2, 1, elem)

#define FMT_TRAITS_I(name, seq) \
    template<> struct fmt::formatter< name > { \
        constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.end(); } \
        template <typename FormatContext> \
        inline auto format(const name& m, FormatContext& ctx) const { \
            return fmt::format_to(ctx.out(), BOOST_PP_STRINGIZE(name) " {{ " \
                BOOST_PP_SEQ_FOR_EACH(FMT_HELPER_STRINGIZE_MEMBER, ~, seq) \
            "}}" \
                BOOST_PP_SEQ_FOR_EACH(FMT_HELPER_MEMBER_AS_ARG, ~, seq) \
            ); \
        } \
    };

#define FMT_TRAITS(name, items) \
    FMT_TRAITS_I(BOOST_PP_TUPLE_ELEM(1, 0, name), BOOST_PP_CAT(FMT_HELPER_WRAP_SEQUENCE_X items, 0))

#define FMT_DECLARE_I(name, seq) \
    struct name { BOOST_PP_SEQ_FOR_EACH(FMT_HELPER_DEFINE_MEMBER, ~, seq) }; FMT_TRAITS_I(name, seq)
 
#define FMT_DECLARE(name, items) \
    FMT_DECLARE_I(BOOST_PP_TUPLE_ELEM(1, 0, name), BOOST_PP_CAT(FMT_HELPER_WRAP_SEQUENCE_X items, 0))

#endif

#ifdef FMT_FORMAT_H_

#include <unordered_map>
#include <variant>
#include <etl/result.h>

template <typename T> 
struct fmt::formatter<Project::etl::Ok<T>> : fmt::formatter<T> {
    template <typename Ctx>
    inline auto format(const Project::etl::Ok<T>& m, Ctx& ctx) const { return format_to(ctx.out(), "Ok: {}", m.data); }
};

template <> 
struct fmt::formatter<Project::etl::Ok<void>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const Project::etl::Ok<void>& m, Ctx& ctx) const { return format_to(ctx.out(), "Ok"); }
};

template <typename E> 
struct fmt::formatter<Project::etl::Err<E>> : fmt::formatter<E> {
    template <typename Ctx>
    inline auto format(const Project::etl::Err<E>& m, Ctx& ctx) const { return format_to(ctx.out(), "Err: {}", m.data); }
};

template <typename T, typename E> 
struct fmt::formatter<Project::etl::Result<T, E>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const Project::etl::Result<T, E>& m, Ctx& ctx) const { 
        if (m.is_ok()) {
            return format_to(ctx.out(), "Ok: {}", m.unwrap());
        } else {
            return format_to(ctx.out(), "Err: {}", m.unwrap_err());
        }
    }
};

template <typename K, typename V> 
struct fmt::formatter<std::unordered_map<K, V>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const std::unordered_map<K, V>& m, Ctx& ctx) const {
        auto it = ctx.out();
        fmt::format_to(it, "{{");  // Opening brace for the map

        bool first = true;
        for (const auto& pair : m) {
            if (!first) {
                fmt::format_to(it, ", ");
            }
            first = false;

            fmt::format_to(it, "{}: {}", pair.first, pair.second);
        }

        return fmt::format_to(it, "}}");  // Closing brace for the map
    }
};

template <typename... Ts>
struct fmt::formatter<std::variant<Ts...>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.end(); }
    template <typename Ctx>
    inline auto format(const std::variant<Ts...>& v, Ctx& ctx) const {
        return std::visit([&ctx](const auto& it) {
            return fmt::formatter<std::decay_t<decltype(it)>>().format(it, ctx);
        }, v);
    }
};

#endif

#endif
