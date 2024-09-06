#include <boost/preprocessor.hpp>
#include <fmt/chrono.h>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/tcp.h>
#include <delameta/file.h>
#include <delameta/opts.h>
#include <delameta/endpoint.h>
#include <delameta/utils.h>
#include <csignal>

using namespace Project;
using namespace Project::delameta;
using etl::Err;
using etl::Ok;

HTTP_DEFINE_OBJECT(app);

static void on_sigint(std::function<void()> fn);

using Headers = std::unordered_map<std::string_view, std::string_view>;
using Queries = std::unordered_map<std::string, std::string>;

template<>
auto Opts::convert_string_into(const std::string& str) -> Result<Headers> {
    if (str == "") return Ok(Headers{});
    return json::deserialize<Headers>(str).except([](const char* err) { return Error{-1, err}; });
}

template<>
auto Opts::convert_string_into(const std::string& str) -> Result<Queries> {
    if (str == "") return Ok(Queries{});
    return json::deserialize<Queries>(str).except([](const char* err) { return Error{-1, err}; });
}

OPTS_MAIN(
    (Delameta, "Delameta API")
    ,
    /*   Type   |  Arg   | Short |   Long   |              Help                |      Default   */
    (URL        , uri    , 'H'   , "host"   , "Specify host and port"          , "localhost:5000")
    (int        , n_sock , 'n'   , "n-sock" , "Number of socket server"        , "4"             )
    (std::string, url_str, 'u'   , "url"    , "Specify URL for HTTP request"   , ""              )

    // for sub command
    (std::string, cmd    , 'c'   , "cmd"    , "Specify sub command (optional)" , ""              )
    (Headers    , args   , 'a'   , "args"   , "Sub command's args"             , ""              )
    (Queries    , queries, 'q'   , "queries", "Sub command's queries"          , ""              )
    (std::string, data   , 'd'   , "data"   , "Sub command's data"             , ""              )
    (std::string, method , 'm'   , "method" , "Sub command's HTTP method"      , ""              )

    // data modifier
    (std::string, input  , 'i'   , "input"  , "Specify input endpoint"         , ""              )
    (std::string, file   , 'f'   , "file"   , "Specify input file"             , ""              )
    (bool       , is_json, 'j'   , "is-json", "Set data type to be json"                         )
    (bool       , is_text, 't'   , "is-text", "Set data type to be plain text"                   )
    (bool       , is_form, 'p'   , "is-form", "Set data type to be form-urlencoded"              )

    // utils
    (std::string, output , 'o'   , "output" , "Specify output endpoint"        , "stdio://"      )
    (bool       , verbose, 'v'   , "verbose", "Set verbosity"                                    )
    ,
    (Result<void>)
) {
    Opts::verbose = verbose;

    // launch http server if cmd and url are not specified
    if (cmd == "" and url_str == "") {
        Server<TCP> tcp_server;

        app.bind(tcp_server, {.is_tcp_server=true});
        on_sigint([&]() { tcp_server.stop(); });

        fmt::println("Server is starting on {}", uri.host);
        return tcp_server.start(FL, {.host=uri.host, .max_socket=n_sock});
    }

    // create request
    http::RequestReader req;
    http::ResponseWriter res;

    req.version = "HTTP/1.1";
    req.headers = std::move(args);
    req.body = std::move(data);

    uri.path = std::move(cmd);
    uri.queries = std::move(queries);
    req.url = std::move(uri);

    // setup body
    if ((not req.body.empty()) + (not input.empty()) + (not file.empty()) > 1) {
        return Err(Error{-1, "multiple data source"});
    }

    if (input != "") {
        auto ep_in = TRY(Endpoint::Open(FL, input));
        ep_in >> req.body_stream;
    } else if (file != "") {
        auto f = TRY(File::Open(FL, {file}));
        f >> req.body_stream;
        req.headers["Content-Type"] = get_content_type_from_file(file);
    }
    if (is_json) {
        req.headers["Content-Type"] = "application/json";
    }
    if (is_text) {
        req.headers["Content-Type"] = "text/plain";
    }
    if (is_form) {
        req.headers["Content-Type"] = "application/x-www-form-urlencoded";
    }

    if (method == "") {
        req.method = req.body.empty() and req.body_stream.rules.empty() ? "GET" : "POST";
    } else {
        req.method = std::move(method);
    }

    // create response using http request
    if (not url_str.empty()) {
        http::RequestWriter req_w = std::move(req);
        req_w.url = URL(std::move(url_str));

        auto session = TRY(TCP::Open(FL, {req_w.url.host}));
        auto res_r = TRY(http::request(session, std::move(req_w)));

        res = std::move(res_r);
    }
    // create response using cmd from router path
    else {
        auto it = std::find_if(app.routers.begin(), app.routers.end(), [&](const http::Router& r) {
            return r.path == req.url.path;
        });
        if (it == app.routers.end()) {
            return Err(Error{-1, fmt::format("cannot find path '{}'", req.url.path)});
        }
        res.status = 200;
        it->function(req, res);
        if (res.status_string.empty()) res.status_string = http::status_to_string(res.status);
    }

    // output the response
    auto ep_out = TRY(Endpoint::Open(FL, output));
    ep_out << res.body << res.body_stream;

    if (output.starts_with("stdio://")) {
        fmt::println("");
    }

    // return ok or error
    if (res.status < 300) {
        return Ok();
    } else {
        return Err(Error{res.status, res.status_string});
    }
}

static void on_sigint(std::function<void()> fn) {
    static std::function<void()> at_exit;
    at_exit = std::move(fn);
    auto sig = +[](int) { at_exit(); };
    std::signal(SIGINT, sig);
    std::signal(SIGTERM, sig);
    std::signal(SIGQUIT, sig);
}

static auto format_time_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    return fmt::localtime(time_now);
}

namespace Project::delameta {

    void info(const char* file, int line, const std::string& msg) {
        if (Opts::verbose) 
            fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} INFO: {}", format_time_now(), file, line, msg);
    }
    void warning(const char* file, int line, const std::string& msg) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} WARNING: {}", format_time_now(), file, line, msg);
    }
    void panic(const char* file, int line, const std::string& msg) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} PANIC: {}", format_time_now(), file, line, msg);
        exit(1);
    }
}
