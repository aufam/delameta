#ifndef DELAMETA_OPTS_H
#define DELAMETA_OPTS_H

#include "delameta/error.h"
#include <unordered_map>
#include <functional>

namespace Project::delameta {

    class Opts {
    public:
        template <typename T>
        struct Arg {
            char short_;
            const char* long_;
            const char* description;
            const char* default_str = nullptr;
        };

        template <typename ...Ts, typename F> static
        int execute(
            int argc, char** argv, 
            const char* app_name, 
            const char* app_description, 
            std::tuple<Arg<Ts>...> args, 
            F&& handler
        ) {
            return execute_(argc, argv, app_name, app_description, std::move(args), std::function(std::forward<F>(handler)));
        }

        static bool verbose;
    
    protected:
        template <typename R, typename ...Ts> static
        int execute_(
            int argc, char** argv, 
            const char* app_name, 
            const char* app_description, 
            std::tuple<Arg<Ts>...> args, 
            std::function<R(Ts...)> handler
        ) {
            std::unordered_map<char, std::string> collected_arg_values = std::apply([&](const auto&... arg) {
                return collect_arg_values(app_name, app_description, argc, argv, {get_option_flags(arg)...});
            }, args);

            // process each args
            std::tuple<Result<Ts>...> arg_values = std::apply([&](const auto&... arg) {
                return std::tuple { process_arg<Ts>(arg, collected_arg_values)... };
            }, args);

            // check for err
            Error* err = nullptr;
            auto check_err = [&](auto& item) {
                if (err == nullptr && item.is_err()) {
                    err = &item.unwrap_err();
                }
            };
            std::apply([&](auto&... args) { ((check_err(args)), ...); }, arg_values);
            if (err) return handle_error(*err);

            // apply handler
            if constexpr (std::is_void_v<R>) {
                std::apply([&](auto&... args) { handler(std::move(args.unwrap())...); }, arg_values);
            } else {
                R result = std::apply([&](auto&... args) { return handler(std::move(args.unwrap())...); }, arg_values);
                if constexpr (etl::is_etl_result_v<R>) {
                    if (result.is_err()) {
                        return handle_error(result.unwrap_err());
                    }
                    if constexpr (!std::is_void_v<etl::result_value_t<R>>) {
                        return handle_result(result.unwrap());
                    }
                } else {
                    return handle_result(result);
                }
            } 
            print_result(nullptr, 0);
            return 0;
        }

        struct OptionFlags {
            char short_;
            const char* long_;
            const char* description;
            int kind; // no_argument, required_argument, optional_argument
            const char* default_str = nullptr;
        };

        template<typename T> static constexpr OptionFlags
        get_option_flags(const Arg<T>& arg) {
            if constexpr (std::is_same_v<T, bool>) {
                return {arg.short_, arg.long_, arg.description, 0};
            } else {
                return {arg.short_, arg.long_, arg.description, arg.default_str == nullptr ? 1 : 2, arg.default_str};
            }
        }

        static std::unordered_map<char, std::string>
        collect_arg_values(
            const char* app_name, 
            const char* app_description, 
            int argc, char** argv, 
            std::initializer_list<OptionFlags>&& flags
        );

        template <typename T>
        struct always_false : std::false_type {};

        template <typename T> static Result<T> 
        process_arg(const Arg<T>& arg, const std::unordered_map<char, std::string>& collected_arg_values) {
            auto it = collected_arg_values.find(arg.short_);
            if constexpr (std::is_same_v<T, bool>) {
                return etl::Ok(it != collected_arg_values.end());
            } else {
                if (it == collected_arg_values.end()) {
                    if (arg.default_str != nullptr) {
                        return convert_string_into<T>(arg.default_str);
                    } else {
                        return etl::Err(Error{1, std::string("-") + arg.short_ + " --" + arg.long_ + " is not specified"});
                    }
                } else {
                    return convert_string_into<T>(it->second);
                }
            }
        }

        template <typename T> static Result<T> 
        convert_string_into(const std::string& str) {
            if constexpr (std::is_integral_v<T>) {
                try {
                    return etl::Ok(std::stoi(str));
                } catch (const std::exception&) {
                    return etl::Err(Error{1, "cannot covert " + str + " to int"});
                }
            } else if constexpr (std::is_floating_point_v<T>) {
                try {
                    return etl::Ok(std::stof(str));
                } catch (const std::exception&) {
                    return etl::Err(Error{1, "cannot covert " + str + " to float"});
                }
            } else if constexpr (std::is_convertible_v<std::string, T>) {
                return etl::Ok(str);
            } else if constexpr (std::is_convertible_v<std::string_view, T>) {
                return etl::Ok(std::string_view(str));
            } else if constexpr (std::is_convertible_v<const char*, T>) {
                return etl::Ok(str.c_str());
            } else {
                static_assert(always_false<T>::value);
            }
        }

        template <typename T> static int
        handle_result(const T& result) {
            if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
                print_result(result.data(), result.size());
            } else if constexpr (std::is_same_v<T, const char*>) {
                print_result(result, -1);
            } else {
                std::string str = std::to_string(result);
                return handle_result(str);
            }
            return 0;
        }

        static int handle_error(const Error& err);
    
    private:
        static void print_result(const char* ptr, int len);
    };
}

#ifdef BOOST_PREPROCESSOR_HPP

#define OPTS_HELPER_VARIADIC(...) __VA_ARGS__

#define OPTS_HELPER_WRAP_SEQUENCE_X(...) ((__VA_ARGS__)) OPTS_HELPER_WRAP_SEQUENCE_Y
#define OPTS_HELPER_WRAP_SEQUENCE_Y(...) ((__VA_ARGS__)) OPTS_HELPER_WRAP_SEQUENCE_X
#define OPTS_HELPER_WRAP_SEQUENCE_X0
#define OPTS_HELPER_WRAP_SEQUENCE_Y0

#define OPTS_HELPER_DEFINE_OPT_ARG(r, data, elem) \
    ::Project::delameta::Opts::Arg<BOOST_PP_TUPLE_ELEM(6, 0, elem)> { \
    BOOST_PP_TUPLE_ELEM(6, 2, elem), \
    BOOST_PP_TUPLE_ELEM(6, 3, elem), \
    BOOST_PP_TUPLE_ELEM(6, 4, elem), \
    BOOST_PP_TUPLE_ELEM(6, 5, elem) },

#define OPTS_HELPER_DEFINE_FN_ARG(r, data, elem) \
    BOOST_PP_TUPLE_ELEM(6, 0, elem) BOOST_PP_TUPLE_ELEM(6, 1, elem),

#define OPTS_HELPER_DEFINE_FN(name, args, ret) \
    OPTS_HELPER_VARIADIC ret name BOOST_PP_TUPLE_POP_BACK((BOOST_PP_SEQ_FOR_EACH(OPTS_HELPER_DEFINE_FN_ARG, ~, args) void))

#define OPTS_MAIN_I(name, desc, args, ret)\
    OPTS_HELPER_DEFINE_FN(name, args, ret); \
    [[maybe_unused]] int __argc; [[maybe_unused]] char** __argv; \
    int main(int argc, char** argv) { \
        __argc = argc; __argv = argv; \
        return ::Project::delameta::Opts::execute(argc, argv, BOOST_PP_STRINGIZE(name), desc, \
        std::tuple {\
            BOOST_PP_SEQ_FOR_EACH(OPTS_HELPER_DEFINE_OPT_ARG, ~, args) \
        }, name); \
    } \
    OPTS_HELPER_DEFINE_FN(name, args, ret)

#define OPTS_MAIN(nd, args, ret) \
    OPTS_MAIN_I(BOOST_PP_TUPLE_ELEM(2, 0, nd), BOOST_PP_TUPLE_ELEM(2, 1, nd), \
    BOOST_PP_CAT(OPTS_HELPER_WRAP_SEQUENCE_X args, 0), ret)

#endif

#endif