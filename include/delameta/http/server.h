#ifndef PROJECT_HTTP_SERVER_H
#define PROJECT_HTTP_SERVER_H

#include "delameta/http/request.h"
#include "delameta/http/response.h"
#include "etl/json_serialize.h"
#include "etl/json_deserialize.h"

namespace Project::delameta::http {

    class Server : public Movable {
    public:
        Server() = default;
        virtual ~Server() = default;

        Server(Server&&) noexcept = default;
        Server& operator=(Server&&) noexcept = default;

        template <typename R, typename... Args>
        using Handler = std::function<R(Args..., const RequestReader&, ResponseWriter&)>;

        struct Error {
            int status;
            std::string what;

            Error(int status);
            Error(int status, std::string what);
            Error(delameta::Error);
        };

        template <typename T>
        using Result = etl::Result<T, Error>;

        struct Router {
            std::string path;
            std::vector<const char*> methods;
            Handler<void> function;
        };

        struct Arg { const char* name; };

        template <typename T>
        struct ArgDefaultVal { const char* name; T default_value; };

        template <typename F>
        struct ArgDefaultFn { const char* name; F default_fn; };

        struct ArgJsonItem { const char* key; };

        template <typename F>
        struct ArgJsonItemDefaultVal { const char* key; F default_value; };
    
        template <typename F>
        struct ArgJsonItemDefaultFn { const char* key; F default_fn; };

        template <typename F>
        struct ArgDepends { F depends; };

        struct ArgRequest {};
        struct ArgResponse {};
        struct ArgMethod {};
        struct ArgURL {};
        struct ArgHeaders {};
        struct ArgQueries {};
        struct ArgPath {};
        struct ArgFullPath {};
        struct ArgFragment {};
        struct ArgVersion {};
        struct ArgBody {};
        struct ArgJson {};
        struct ArgText {};
    
        template <typename T> struct is_server_result : std::false_type {};
        template <typename T, typename E> struct is_server_result<etl::Result<T, E>> : std::is_convertible<E, Error> {};
        template <typename T> static constexpr bool is_server_result_v = is_server_result<T>::value;

        template <typename T> struct is_handler : is_handler<decltype(std::function(std::declval<T>()))> {};
        template <typename T> struct is_handler<std::function<T(const RequestReader&, ResponseWriter&)>> : std::true_type {};
        template <typename T> static constexpr bool is_handler_v = is_handler<T>::value;

        template <typename... Args, typename F> 
        auto route(std::string path, std::vector<const char*> methods, std::tuple<Args...> args, F&& handler) {
            return route_(std::move(path), std::move(methods), std::move(args), std::function(std::forward<F>(handler)));
        }

        Result<void> reroute(std::string path, etl::Ref<const RequestReader> req, etl::Ref<ResponseWriter> res);
        
        template <typename... Args, typename F> 
        auto Get(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"GET"}, std::move(args), std::forward<F>(handler));
        }

        template <typename... Args, typename F> 
        auto Post(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"POST"}, std::move(args), std::forward<F>(handler));
        }

        template <typename... Args, typename F>  
        auto Patch(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"PATCH"}, std::move(args), std::forward<F>(handler));
        }

        template <typename... Args, typename F>  
        auto Put(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"PUT"}, std::move(args), std::forward<F>(handler));
        }

        template <typename... Args, typename F> 
        auto Head(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"HEAD"}, std::move(args), std::forward<F>(handler));
        }

        template <typename... Args, typename F> 
        auto Trace(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"Trace"}, std::move(args), std::forward<F>(handler));
        }

        template <typename... Args, typename F> 
        auto Delete(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"DELETE"}, std::move(args), std::forward<F>(handler));
        }

        template <typename... Args, typename F> 
        auto Options(std::string path, std::tuple<Args...> args, F&& handler) {
            return route(std::move(path), {"OPTIONS"}, std::move(args), std::forward<F>(handler));
        }

        std::unordered_map<std::string, Handler<std::string>> global_headers;
        std::list<Handler<Result<void>>> preconditions;
        Handler<void, const std::string&> logger = {};
        Handler<void, Error> error_handler = default_error_handler;
        std::list<Router> routers;
        bool show_response_time = false;

        void bind(StreamSessionServer& server) const;
        std::pair<RequestReader, ResponseWriter> execute(Descriptor& desc, const std::vector<uint8_t>& data) const;
    
    protected:
        template <typename... RouterArgs, typename R, typename ...HandlerArgs>
        auto route_(
            std::string path, 
            std::vector<const char*> methods, 
            std::tuple<RouterArgs...> args,
            std::function<R(HandlerArgs...)> handler
        ) {
            static_assert(sizeof...(RouterArgs) == sizeof...(HandlerArgs));

            Handler<void> function = [this, args=std::move(args), handler] (const RequestReader& req, ResponseWriter& res) {
                for (auto &fn : preconditions) {
                    auto [_, err] = fn(req, res);
                    if (err) return error_handler(std::move(*err), req, res);
                }

                // process each args
                std::tuple<Result<HandlerArgs>...> arg_values = std::apply([&](const auto&... items) {
                    return std::tuple { process_arg<HandlerArgs>(items, req, res)... };
                }, args);

                // check for err
                Error* err = nullptr;
                auto check_err = [&](auto& item) {
                    if (err == nullptr && item.is_err()) {
                        err = &item.unwrap_err();
                    }
                };
                std::apply([&](auto&... args) { ((check_err(args)), ...); }, arg_values);
                if (err) return error_handler(std::move(*err), req, res);
               
                // apply handler
                if constexpr (std::is_void_v<R>) {
                    std::apply([&](auto&... args) { handler(std::move(args.unwrap())...); }, arg_values);
                } else {
                    R result = std::apply([&](auto&... args) { return handler(std::move(args.unwrap())...); }, arg_values);
                    if constexpr (is_server_result_v<R>) {
                        if (result.is_err()) {
                            return error_handler(std::move(result.unwrap_err()), req, res);
                        }
                        if constexpr (!std::is_void_v<etl::result_value_t<R>>) {
                            process_result(result.unwrap(), req, res);
                        }
                    } else {
                        process_result(result, req, res);
                    }
                } 
            };

            routers.push_back(Router{std::move(path), std::move(methods), std::move(function)});
            return handler;
        }

        static void default_error_handler(Error err, const RequestReader&, ResponseWriter& res) {
            res.status = err.status;
            res.body = std::move(err.what);
        }

        static Error internal_error(const char* err) {
            return {StatusInternalServerError, std::string(err)};
        }

        template <typename T>
        struct always_false : std::false_type {};

        #define DELAMETA_HTTP_SERVER_PROCESS_ARG(arg) \
            if (auto it = req.headers.find(arg); it != req.headers.end()) \
                return convert_string_into<T>(it->second); \
            if (auto it = req.url.queries.find(arg); it != req.url.queries.end()) \
                return convert_string_into<T>(it->second); \

        template <typename T> static Result<T>
        process_arg(const Arg& arg, const RequestReader& req, ResponseWriter&) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG(arg.name);
            return etl::Err(Error{StatusBadRequest, std::string() + "arg '" + arg.name + "' not found"});
        }

        template <typename T, typename U> static Result<T>
        process_arg(const ArgDefaultVal<U>& arg, const RequestReader& req, ResponseWriter&) {
            static_assert(std::is_convertible_v<U, T>);
            DELAMETA_HTTP_SERVER_PROCESS_ARG(arg.name);
            return etl::Ok(arg.default_value);
        }

        template <typename T, typename F> static Result<T>
        process_arg(const ArgDefaultFn<F>& arg, const RequestReader& req, ResponseWriter& res) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG(arg.name);
            using U = decltype(arg.default_fn(req, res));
            if constexpr (is_server_result_v<U>) {
                return arg.default_fn(req, res);
            } else if constexpr (std::is_convertible_v<U, T>) {
                return etl::Ok(arg.default_fn(req, res));
            } else {
                static_assert(always_false<U>::value);
            }
        }

        #undef DELAMETA_HTTP_SERVER_PROCESS_ARG

        #define DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(key) \
            if (req.body.empty()) req.body_stream >> [&req](std::string_view chunk) { req.body += chunk; }; \
            if (get_content_type(req) != "application/json") \
                return etl::Err(Error{StatusBadRequest, "Content-Type is not json"}); \
            auto j = etl::Json::parse(etl::string_view(req.body.data(), req.body.size())); \
            auto err_msg = j.error_message(); \
            if (err_msg) \
                return etl::Err(Error{StatusBadRequest, std::string(err_msg.data())}); \
            if (!j.is_dictionary()) \
                return etl::Err(Error{StatusBadRequest, "JSON is not a map"}); \
            auto item = j[key]; \
            err_msg = item.error_message(); \
            if (!err_msg) \
                return etl::json::deserialize<T>(item.dump()).except(internal_error); \


        template <typename T> static Result<T>
        process_arg(const ArgJsonItem& arg, const RequestReader& req, ResponseWriter&) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(arg.key)
            if constexpr(etl::detail::is_std_optional_v<T>)
                return etl::Ok(std::nullopt);
            if constexpr(etl::is_optional_v<T>)
                return etl::Ok(etl::none);
            return etl::Err(Error{StatusBadRequest, std::string(err_msg.data())});
        }

        template <typename T, typename U> static Result<T>
        process_arg(const ArgJsonItemDefaultVal<U>& arg, const RequestReader& req, ResponseWriter&) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(arg.key)
            return etl::Ok(arg.default_value);
        }

        template <typename T, typename F> static Result<T>
        process_arg(const ArgJsonItemDefaultFn<F>& arg, const RequestReader& req, ResponseWriter& res) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(arg.key)
            using U = decltype(arg.default_fn(req, res));
            if constexpr (is_server_result_v<U>) {
                return arg.default_fn(req, res);
            } else if constexpr (std::is_convertible_v<U, T>) {
                return etl::Ok(arg.default_fn(req, res));
            } else {
                static_assert(always_false<U>::value);
            }
        }

        #undef DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON

        template <typename T, typename F> static Result<T>
        process_arg(const ArgDepends<F>& arg, const RequestReader& req, ResponseWriter& res) {
            using U = decltype(arg.depends(req, res));
            if constexpr (is_server_result_v<U>) {
                return arg.depends(req, res);
            } else if constexpr (std::is_convertible_v<U, T>) {
                return etl::Ok(arg.depends(req, res));
            } else {
                static_assert(always_false<U>::value);
            }
        }
        
        template <typename T> static Result<T>
        process_arg(const ArgRequest&, const RequestReader& req, ResponseWriter&) {
            static_assert(std::is_same_v<T, etl::Ref<const RequestReader>>);
            return etl::Ok(etl::ref_const(req));
        }
        
        template <typename T> static Result<T>
        process_arg(const ArgResponse&, const RequestReader&, ResponseWriter& res) {
            static_assert(std::is_same_v<T, etl::Ref<ResponseWriter>>);
            return etl::Ok(etl::ref(res));
        }

        template <typename T> static Result<T>
        process_arg(const ArgMethod&, const RequestReader& req, ResponseWriter&) {
            return convert_string_into<T>(req.method);
        }

        template <typename T> static Result<T>
        process_arg(const ArgURL&, const RequestReader& req, ResponseWriter&) {
            static_assert(std::is_same_v<T, etl::Ref<const decltype(RequestReader::url)>>);
            return etl::Ok(etl::ref_const(req.url));
        }

        template <typename T> static Result<T>
        process_arg(const ArgHeaders&, const RequestReader& req, ResponseWriter&) {
            static_assert(std::is_same_v<T, etl::Ref<const decltype(RequestReader::headers)>>);
            return etl::Ok(etl::ref_const(req.headers));
        }

        template <typename T> static Result<T>
        process_arg(const ArgQueries&, const RequestReader& req, ResponseWriter&) {
            static_assert(std::is_same_v<T, etl::Ref<const decltype(URL::queries)>>);
            return etl::Ok(etl::ref_const(req.url.queries));
        }

        template <typename T> static Result<T>
        process_arg(const ArgPath&, const RequestReader& req, ResponseWriter&) {
            return convert_string_into<T>(req.url.path);
        }

        template <typename T> static Result<T>
        process_arg(const ArgFullPath&, const RequestReader& req, ResponseWriter&) {
            return convert_string_into<T>(req.url.full_path);
        }

        template <typename T> static Result<T>
        process_arg(const ArgFragment&, const RequestReader& req, ResponseWriter&) {
            return convert_string_into<T>(req.url.fragment);
        }

        template <typename T> static Result<T>
        process_arg(const ArgVersion&, const RequestReader& req, ResponseWriter&) {
            return convert_string_into<T>(req.version);
        }

        template <typename T> static Result<T>
        process_arg(const ArgBody&, const RequestReader& req, ResponseWriter&) {
            return convert_stream_into<T>(req);
        }

        template <typename T> static Result<T>
        process_arg(const ArgJson&, const RequestReader& req, ResponseWriter&) {
            if (get_content_type(req) == "application/json")
                return convert_stream_into<T>(req);
            else
                return etl::Err(Error{StatusBadRequest, std::string() + "Content-Type is not json"});
        }

        template <typename T> static Result<T>
        process_arg(const ArgText&, const RequestReader& req, ResponseWriter&) {
            if (get_content_type(req) == "text/plain")
                return convert_stream_into<T>(req);
            else
                return etl::Err(Error{StatusBadRequest, std::string() + "Content-Type is not text/plain"});
        }

        template <typename T> static void
        process_result(T& result, const RequestReader&, ResponseWriter& res) {
            if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<T, const char*>) {
                res.body = std::move(result);
                res.headers["Content-Type"] = "text/plain";
            } else if constexpr (etl::is_etl_string_v<T> || std::is_same_v<T, etl::StringView>) {
                res.body = std::string(result.data(), result.len());
                res.headers["Content-Type"] = "text/plain";
            } else if constexpr (std::is_same_v<T, ResponseWriter>) {
                res = std::move(result);
            } else if constexpr (std::is_same_v<T, ResponseReader>) {
                res = result;
            } else if constexpr (std::is_same_v<T, delameta::Stream>) {
                res.body_stream = std::move(result);
            } else {
                res.body = etl::json::serialize(result);
                res.headers["Content-Type"] = "application/json";
            }
        }

        static std::string_view
        get_content_type(const RequestReader& req) {
            auto it = req.headers.find("Content-Type");
            if (it == req.headers.end()) {
                it = req.headers.find("content-type");
            }
            if (it != req.headers.end()) {
                return it->second;
            } else {
                return "";
            }
        }

        template <typename T> static Result<T> 
        convert_string_into(std::string_view str) {
            if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<T, etl::StringView>) {
                return etl::Ok(T(str.data(), str.size()));
            } else if constexpr (etl::is_etl_string_v<T>) {
                return etl::Ok(T("%.*s", str.size(), str.data()));
            } else if constexpr (std::is_same_v<T, const char*>) {
                return etl::Ok(str.data());
            } else {
                return etl::json::deserialize<T>(str).except(internal_error);
            }
        }

        template <typename T> static Result<T> 
        convert_stream_into(const RequestReader& req) {
            if constexpr (std::is_same_v<T, delameta::Stream>) {
                return etl::Ok(std::move(req.body_stream));
            } else {
                if (req.body.empty()) req.body_stream >> [&req](std::string_view chunk) { req.body += chunk; };
                return convert_string_into<T>(req.body);
            }
        }
    };
}


namespace Project::delameta::http::arg {
    inline Server::Arg arg(const char* name) { return {name}; }
    inline Server::ArgJsonItem json_item(const char* key) { return {key}; }

    template <typename F>
    auto depends(F&& depends_function) {
        static_assert(Server::is_handler_v<std::decay_t<F>>, 
            "The function signature should be R(const RequestReader&, ResponseWriter&)");
        return Server::ArgDepends<F> { std::move(std::forward<F>(depends_function)) };
    }

    template <typename T>
    auto default_val(const char* name, T&& default_value) {
        return Server::ArgDefaultVal<T> { name, std::forward<T>(default_value) };
    }

    template <typename F>
    auto default_fn(const char* name, F&& default_function) {
        static_assert(Server::is_handler_v<std::decay_t<F>>, 
            "The function signature should be R(const RequestReader&, ResponseWriter&)");
        return Server::ArgDefaultFn<F> { name, std::forward<F>(default_function) };
    }

    template <typename T>
    auto json_item_default_val(const char* key, T&& default_value) {
        return Server::ArgJsonItemDefaultVal<T> { key, std::forward<T>(default_value) };
    }

    template <typename F>
    auto json_item_default_fn(const char* key, F&& default_function) {
        static_assert(Server::is_handler_v<std::decay_t<F>>, 
            "The function signature should be R(const RequestReader&, ResponseWriter&)");
        return Server::ArgJsonItemDefaultFn<F> { key, std::forward<F>(default_function) };
    }

    inline static constexpr Server::ArgRequest request {};
    inline static constexpr Server::ArgResponse response {};
    inline static constexpr Server::ArgURL url {};
    inline static constexpr Server::ArgHeaders headers {};
    inline static constexpr Server::ArgQueries queries {};
    inline static constexpr Server::ArgPath path {};
    inline static constexpr Server::ArgFullPath full_path {};
    inline static constexpr Server::ArgFragment fragment {};
    inline static constexpr Server::ArgVersion version {};
    inline static constexpr Server::ArgMethod method {};
    inline static constexpr Server::ArgBody body {};
    inline static constexpr Server::ArgJson json {};
    inline static constexpr Server::ArgText text {};
}

#endif
