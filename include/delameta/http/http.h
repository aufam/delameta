#ifndef PROJECT_DELAMETA_HTTP_HTTP_H
#define PROJECT_DELAMETA_HTTP_HTTP_H

#include "delameta/http/request.h"
#include "delameta/http/response.h"
#include "delameta/http/arg.h"
#include "delameta/http/error.h"
#include "delameta/json.h"

namespace Project::delameta::http {

    delameta::Result<ResponseReader> request(StreamSessionClient& session, RequestWriter req);
    delameta::Result<ResponseReader> request(StreamSessionClient&& session, RequestWriter req);
    delameta::Result<ResponseReader> request(RequestWriter req);

    template <typename R, typename... Args>
    using Handler = std::function<R(Args..., const RequestReader&, ResponseWriter&)>;

    struct Router {
        std::string path;
        std::vector<const char*> methods;
        Handler<void> function;
    };

    template <typename T> struct is_handler : is_handler<decltype(std::function(std::declval<T>()))> {};
    template <typename T> struct is_handler<std::function<T(const RequestReader&, ResponseWriter&)>> : std::true_type {};
    template <typename T> static constexpr bool is_handler_v = is_handler<T>::value;
    
    class Http : public Movable {
    public:
        Http() = default;
        virtual ~Http() = default;

        Http(Http&&) noexcept = default;
        Http& operator=(Http&&) noexcept = default;

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
            return route(std::move(path), {"TRACE"}, std::move(args), std::forward<F>(handler));
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

        struct BindArg {
            bool is_tcp_server;
        };

        void bind(StreamSessionServer& server, BindArg is_tcp_server = {false}) const;
        std::pair<RequestReader, ResponseWriter> execute(Descriptor& desc, const std::vector<uint8_t>& data) const;
    
    protected:
        struct Context {
            std::string_view content_type;
            etl::Json json;
            std::unordered_map<std::string, std::string> form;

            enum Type { Any, JSON, Form };
            Type type = Any;

            Context(const RequestReader& req);
            bool content_type_starts_with(std::string_view prefix) const;
            Result<std::string_view> form_at(const char* key) const;
        };

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

                Context ctx(req);

                // process each args
                std::tuple<Result<HandlerArgs>...> arg_values = std::apply([&](const auto&... items) {
                    return std::tuple { process_arg<HandlerArgs>(items, req, res, ctx)... };
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
        process_arg(const Arg& arg, const RequestReader& req, ResponseWriter&, Context&) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG(arg.name);
            return etl::Err(Error{StatusBadRequest, std::string() + "arg '" + arg.name + "' not found"});
        }

        template <typename T, typename U> static Result<T>
        process_arg(const ArgDefaultVal<U>& arg, const RequestReader& req, ResponseWriter&, Context&) {
            static_assert(std::is_convertible_v<U, T>);
            DELAMETA_HTTP_SERVER_PROCESS_ARG(arg.name);
            return etl::Ok(arg.default_value);
        }

        template <typename T, typename F> static Result<T>
        process_arg(const ArgDefaultFn<F>& arg, const RequestReader& req, ResponseWriter& res, Context&) {
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

        #define DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(key, ctx) \
            if (ctx.type != Context::JSON) \
                return etl::Err(Error{StatusBadRequest, "Content-Type is not json"}); \
            auto err_msg = ctx.json.error_message(); \
            if (err_msg) \
                return etl::Err(Error{StatusBadRequest, std::string(err_msg.data())}); \
            if (!ctx.json.is_dictionary()) \
                return etl::Err(Error{StatusBadRequest, "JSON is not a map"}); \
            auto item = ctx.json[key]; \
            err_msg = item.error_message(); \
            if (!err_msg) \
                return etl::json::deserialize<T>(item.dump()).except(internal_error); \


        template <typename T> static Result<T>
        process_arg(const ArgJsonItem& arg, const RequestReader&, ResponseWriter&, Context& ctx) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(arg.key, ctx)
            if constexpr(etl::detail::is_std_optional_v<T>)
                return etl::Ok(std::nullopt);
            if constexpr(etl::is_optional_v<T>)
                return etl::Ok(etl::none);
            return etl::Err(Error{StatusBadRequest, std::string(err_msg.data())});
        }

        template <typename T, typename U> static Result<T>
        process_arg(const ArgJsonItemDefaultVal<U>& arg, const RequestReader&, ResponseWriter&, Context& ctx) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(arg.key, ctx)
            return etl::Ok(arg.default_value);
        }

        template <typename T, typename F> static Result<T>
        process_arg(const ArgJsonItemDefaultFn<F>& arg, const RequestReader& req, ResponseWriter& res, Context& ctx) {
            DELAMETA_HTTP_SERVER_PROCESS_ARG_JSON(arg.key, ctx)
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
        process_arg(const ArgDepends<F>& arg, const RequestReader& req, ResponseWriter& res, Context&) {
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
        process_arg(const ArgRequest&, const RequestReader& req, ResponseWriter&, Context&) {
            static_assert(std::is_same_v<T, etl::Ref<const RequestReader>>);
            return etl::Ok(etl::ref_const(req));
        }
        
        template <typename T> static Result<T>
        process_arg(const ArgResponse&, const RequestReader&, ResponseWriter& res, Context&) {
            static_assert(std::is_same_v<T, etl::Ref<ResponseWriter>>);
            return etl::Ok(etl::ref(res));
        }

        template <typename T> static Result<T>
        process_arg(const ArgMethod&, const RequestReader& req, ResponseWriter&, Context&) {
            return convert_string_into<T>(req.method);
        }

        template <typename T> static Result<T>
        process_arg(const ArgURL&, const RequestReader& req, ResponseWriter&, Context&) {
            static_assert(
                std::is_same_v<T, decltype(RequestReader::url)> || 
                std::is_same_v<T, etl::Ref<const decltype(RequestReader::url)>>
            );
            if constexpr (std::is_same_v<T, decltype(RequestReader::url)>)
                return etl::Ok(req.url);
            else
                return etl::Ok(etl::ref_const(req.url));
        }

        template <typename T> static Result<T>
        process_arg(const ArgHeaders&, const RequestReader& req, ResponseWriter&, Context&) {
            static_assert(
                std::is_same_v<T, decltype(RequestReader::headers)> ||
                std::is_same_v<T, etl::Ref<const decltype(RequestReader::headers)>>
            );
            if constexpr (std::is_same_v<T, decltype(RequestReader::headers)>)
                return etl::Ok(req.headers);
            else 
                return etl::Ok(etl::ref_const(req.headers));
        }

        template <typename T> static Result<T>
        process_arg(const ArgQueries&, const RequestReader& req, ResponseWriter&, Context&) {
            static_assert(
                std::is_same_v<T, decltype(URL::queries)> ||
                std::is_same_v<T, etl::Ref<const decltype(URL::queries)>>
            );
            if constexpr (std::is_same_v<T, decltype(URL::queries)>)
                return etl::Ok(req.url.queries);
            else 
                return etl::Ok(etl::ref_const(req.url.queries));
        }

        template <typename T> static Result<T>
        process_arg(const ArgPath&, const RequestReader& req, ResponseWriter&, Context&) {
            return convert_string_into<T>(req.url.path);
        }

        template <typename T> static Result<T>
        process_arg(const ArgFullPath&, const RequestReader& req, ResponseWriter&, Context&) {
            return convert_string_into<T>(req.url.full_path);
        }

        template <typename T> static Result<T>
        process_arg(const ArgFragment&, const RequestReader& req, ResponseWriter&, Context&) {
            return convert_string_into<T>(req.url.fragment);
        }

        template <typename T> static Result<T>
        process_arg(const ArgVersion&, const RequestReader& req, ResponseWriter&, Context&) {
            return convert_string_into<T>(req.version);
        }

        template <typename T> static Result<T>
        process_arg(const ArgBody&, const RequestReader& req, ResponseWriter&, Context&) {
            return convert_stream_into<T>(req);
        }

        template <typename T> static Result<T>
        process_arg(const ArgJson&, const RequestReader&, ResponseWriter&, Context& ctx) {
            if (ctx.type == Context::JSON)
                return etl::json::deserialize<T>(ctx.json).except(internal_error);
            else
                return etl::Err(Error{StatusBadRequest, "Content-Type is not json"});
        }

        template <typename T> static Result<T>
        process_arg(const ArgFormItem& arg, const RequestReader&, ResponseWriter&, Context& ctx) {
            if (ctx.type == Context::Form)
                return ctx.form_at(arg.key).and_then(convert_string_into<T>);
            else
                return etl::Err(Error{StatusBadRequest, "Content-Type is not url-encoded"});
        }

        template <typename T> static Result<T>
        process_arg(const ArgText&, const RequestReader& req, ResponseWriter&, Context& ctx) {
            if (ctx.content_type_starts_with("text/plain"))
                return convert_stream_into<T>(req);
            else
                return etl::Err(Error{StatusBadRequest, "Content-Type is not text/plain"});
        }

        template <typename T> static void
        process_result(T& result, const RequestReader&, ResponseWriter& res) {
            auto ct = res.headers.find("Content-Type");
            if (ct == res.headers.end()) {
                ct = res.headers.find("content-type");
            }

            if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<T, const char*>) {
                res.body = std::move(result);
                if (ct == res.headers.end()) res.headers["Content-Type"] = "text/plain";
            } else if constexpr (etl::is_etl_string_v<T> || std::is_same_v<T, etl::StringView>) {
                res.body = std::string(result.data(), result.len());
                if (ct == res.headers.end()) res.headers["Content-Type"] = "text/plain";
            } else if constexpr (std::is_arithmetic_v<T>) {
                res.body = etl::json::serialize(result);
                if (ct == res.headers.end()) res.headers["Content-Type"] = "text/plain";
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                res.body_stream << std::move(result);
                if (ct == res.headers.end()) res.headers["Content-Type"] = "application/octet-stream";
            } else if constexpr (std::is_same_v<T, ResponseWriter>) {
                res = std::move(result);
            } else if constexpr (std::is_same_v<T, ResponseReader>) {
                res = result;
            } else if constexpr (std::is_same_v<T, Stream>) {
                res.body_stream = std::move(result);
            } else {
                if (ct == res.headers.end()) res.headers["Content-Type"] = "application/json";
                res.headers["Content-Length"] = std::to_string(etl::json::size_max(result));
                res.body_stream = delameta::json::serialize_as_stream(std::move(result));
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
            if constexpr (std::is_same_v<T, Stream>) {
                return etl::Ok(std::move(req.body_stream));
            } else {
                if (req.body.empty()) req.body_stream >> [&req](std::string_view chunk) { req.body += chunk; };
                return convert_string_into<T>(req.body);
            }
        }
    };
}


namespace Project::delameta::http::arg {
    inline Arg arg(const char* name) { return {name}; }
    inline ArgJsonItem json_item(const char* key) { return {key}; }
    inline ArgFormItem form(const char* key) { return {key}; }

    template <typename F>
    auto depends(F&& depends_function) {
        static_assert(is_handler_v<std::decay_t<F>>, 
            "The function signature should be R(const RequestReader&, ResponseWriter&)");
        return ArgDepends<F> { std::move(std::forward<F>(depends_function)) };
    }

    template <typename T>
    auto default_val(const char* name, T&& default_value) {
        return ArgDefaultVal<T> { name, std::forward<T>(default_value) };
    }

    template <typename F>
    auto default_fn(const char* name, F&& default_function) {
        static_assert(is_handler_v<std::decay_t<F>>, 
            "The function signature should be R(const RequestReader&, ResponseWriter&)");
        return ArgDefaultFn<F> { name, std::forward<F>(default_function) };
    }

    template <typename T>
    auto json_item_default_val(const char* key, T&& default_value) {
        return ArgJsonItemDefaultVal<T> { key, std::forward<T>(default_value) };
    }

    template <typename F>
    auto json_item_default_fn(const char* key, F&& default_function) {
        static_assert(is_handler_v<std::decay_t<F>>, 
            "The function signature should be R(const RequestReader&, ResponseWriter&)");
        return ArgJsonItemDefaultFn<F> { key, std::forward<F>(default_function) };
    }

    inline static constexpr ArgRequest request {};
    inline static constexpr ArgResponse response {};
    inline static constexpr ArgURL url {};
    inline static constexpr ArgHeaders headers {};
    inline static constexpr ArgQueries queries {};
    inline static constexpr ArgPath path {};
    inline static constexpr ArgFullPath full_path {};
    inline static constexpr ArgFragment fragment {};
    inline static constexpr ArgVersion version {};
    inline static constexpr ArgMethod method {};
    inline static constexpr ArgBody body {};
    inline static constexpr ArgJson json {};
    inline static constexpr ArgText text {};
}

#ifdef BOOST_PREPROCESSOR_HPP

#define HTTP_EXTERN_OBJECT(o) \
    extern ::Project::delameta::http::Http o; static auto& _http_server = o

#define HTTP_DEFINE_OBJECT(o) \
    extern ::Project::delameta::http::Http o; [[maybe_unused]] static auto& _http_server = o; \
    ::Project::delameta::http::Http o __attribute__((init_priority(101)))

#define HTTP_LATE_INIT __attribute__((init_priority(102)))

#define HTTP_SETUP(name, o) \
    HTTP_EXTERN_OBJECT(o); \
    static void _##name##_http_setup(); \
    struct _http_setup_##name##t { _http_setup_##name##t() { _##name##_http_setup(); } }; \
    static _http_setup_##name##t _##name##_http_setup_instance HTTP_LATE_INIT; \
    static void _##name##_http_setup()

#define HTTP_HELPER_VARIADIC(...) __VA_ARGS__
#define HTTP_HELPER_MAKE_METHODS(...) std::vector<const char*>{__VA_ARGS__}

#define HTTP_HELPER_WRAP_SEQUENCE_X(...) ((__VA_ARGS__)) HTTP_HELPER_WRAP_SEQUENCE_Y
#define HTTP_HELPER_WRAP_SEQUENCE_Y(...) ((__VA_ARGS__)) HTTP_HELPER_WRAP_SEQUENCE_X
#define HTTP_HELPER_WRAP_SEQUENCE_X0
#define HTTP_HELPER_WRAP_SEQUENCE_Y0

#define HTTP_HELPER_DEFINE_FN_ARG(r, data, elem) \
    BOOST_PP_TUPLE_ELEM(3, 0, elem) BOOST_PP_TUPLE_ELEM(3, 1, elem),

#define HTTP_HELPER_DEFINE_FN(name, args, ret) \
    HTTP_HELPER_VARIADIC ret name BOOST_PP_TUPLE_POP_BACK((BOOST_PP_SEQ_FOR_EACH(HTTP_HELPER_DEFINE_FN_ARG, ~, args) void))

#define HTTP_HELPER_DEFINE_HTTP_ARG(r, data, elem) \
    BOOST_PP_TUPLE_ELEM(3, 2, elem),

#define HTTP_HELPER_CLASS_NAME(name) BOOST_PP_CAT(BOOST_PP_CAT(_http_route_, name), _t)
#define HTTP_HELPER_OBJ_NAME(name) BOOST_PP_CAT(_http_route_, name)

#define HTTP_ROUTE_I(path, methods, name, args, ret) \
    HTTP_HELPER_DEFINE_FN(name, args, ret); \
    class HTTP_HELPER_CLASS_NAME(name) { \
    public: \
        HTTP_HELPER_CLASS_NAME(name)() { \
            _http_server.route(path, HTTP_HELPER_MAKE_METHODS methods, \
            std::tuple{ BOOST_PP_SEQ_FOR_EACH(HTTP_HELPER_DEFINE_HTTP_ARG, ~, args) }, name); \
        } \
    }; \
    static HTTP_HELPER_CLASS_NAME(name) HTTP_HELPER_OBJ_NAME(name) HTTP_LATE_INIT; \
    HTTP_HELPER_DEFINE_FN(name, args, ret) 

#define HTTP_ROUTE(pm, name, args, ret) \
    HTTP_ROUTE_I(BOOST_PP_TUPLE_ELEM(3, 0, pm), BOOST_PP_TUPLE_ELEM(3, 1, pm), BOOST_PP_TUPLE_ELEM(1, 0, name), \
    BOOST_PP_CAT(HTTP_HELPER_WRAP_SEQUENCE_X args, 0), ret)

#endif
#ifdef FMT_FORMAT_H_

template <> 
struct fmt::formatter<Project::delameta::http::Error> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const Project::delameta::http::Error& m, Ctx& ctx) const {
        return fmt::format_to(ctx.out(), "http::Error {{code: {}, what: {}}}", m.status, m.what);
    }
};

#endif
#endif
