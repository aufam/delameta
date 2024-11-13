#include <boost/preprocessor.hpp>
#include <fmt/chrono.h>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/tcp.h>
#include <delameta/tls.h>
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

static auto format_time_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    return fmt::localtime(time_now);
}

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
    (Delameta, "Delameta CLI Tools")
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
    (std::string, file   , 'F'   , "file"   , "Specify input file"             , ""              )
    (bool       , is_json, 'j'   , "is-json", "Set data type to be json"                         )
    (bool       , is_text, 't'   , "is-text", "Set data type to be plain text"                   )
    (bool       , is_form, 'f'   , "is-form", "Set data type to be form-urlencoded"              )

    // output
    (std::string, output , 'o'   , "output" , "Specify output endpoint"        , "stdio://"      )
    (std::string, log    , 'l'   , "log"    , "Set log file"                   , ""              )

    // utils
    (bool       , is_ver , 'V'   , "version", "Print version"                                    )
    (bool       , verbose, 'v'   , "verbose", "Set verbosity"                                    )
    (bool       , isn_lf , 'L'   , "disable-lf", "disable line feed"                             )
    (std::string, cert   , 'C'   , "cert"   , "Set TLS certificate file"       , ""              )
    (std::string, key    , 'K'   , "key"    , "Set TLS key file"               , ""              )
    (bool       , p_heads, 'A'   , "print-headers", "Print response header"                      )
    ,
    (Result<void>)
) {
    if (is_ver) {
        fmt::println(DELAMETA_VERSION);
        return Ok();
    }

    Opts::verbose = verbose;

    static std::function<void()> at_exit;
    signal(SIGINT, +[](int) { if (at_exit) at_exit(); });

    // launch http server if cmd and url are not specified
    if (cmd == "" and url_str == "") {
        if (cert == "" and key == "") {
            Server<TCP> tcp;

            app.bind(tcp, {.is_tcp_server=true});
            at_exit = [&] { tcp.stop(); };

            fmt::println("Server is starting on {}", uri.host);
            return tcp.start(FL, {.host=uri.host, .max_socket=n_sock});
        } else if (cert != "" and key != "") {
            Server<TLS> tls;

            app.bind(tls, {.is_tcp_server=true});
            at_exit = [&] { tls.stop(); };

            fmt::println("Server is starting on {}", uri.host);
            return tls.start(FL, {.host=uri.host, .cert_file=cert, .key_file=key, .max_socket=n_sock});
        } else if (cert == "") {
            return Err(Error{-1, "TLS certificate file is not provided"});
        } else if (key == "") {
            return Err(Error{-1, "TLS key file is not provided"});
        }
    }

    // setup request/response
    http::RequestReader req;
    std::string content_length;

    req.version = "HTTP/1.1";
    req.headers = std::move(args);

    uri.path = std::move(cmd);
    uri.queries = std::move(queries);
    req.url = std::move(uri);

    // setup body
    if ((not data.empty()) + (not input.empty()) + (not file.empty()) > 1) {
        return Err(Error{-1, "multiple data source"});
    }
    if (data != "") {
        req.headers["Content-Length"] = content_length = std::to_string(data.size());
        req.body_stream << std::move(data);
    } else if (input != "") {
        auto ep_in = TRY(Endpoint::Open(FL, input));
        auto read_data = TRY(ep_in.read());
        req.headers["Content-Length"] = content_length = std::to_string(read_data.size());
        req.body_stream << std::move(read_data);
    } else if (file != "") {
        auto f = TRY(File::Open(FL, {file}));
        req.headers["Content-Length"] = content_length = std::to_string(f.file_size());
        req.headers["Content-Type"] = get_content_type_from_file(file);
        f >> req.body_stream;
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
        req.method = req.body_stream.rules.empty() ? "GET" : "POST";
    } else {
        req.method = std::move(method);
    }

    // create response using http request
    http::ResponseWriter res;
    if (not url_str.empty()) {
        extern std::string delameta_https_cert_file;
        extern std::string delameta_https_key_file;

        delameta_https_cert_file = cert;
        delameta_https_key_file = key;

        http::RequestWriter req_w = std::move(req);
        req_w.url = URL(std::move(url_str));

        auto res_r = TRY(http::request(std::move(req_w)));
        res = std::move(res_r);
    }
    // create response using cmd from router path
    else {
        app.execute(req, res);
    }

    if (p_heads) {
        for (auto &[key, value]: res.headers) {
            fmt::println("{}: {}", key, value);
        }
    }

    // return ok or error
    if (res.status < 300) {
        // output the response
        auto ep_out = TRY(Endpoint::Open(FL, output));
        File* p_log_out = nullptr;
        if (log != "") {
            auto log_out = TRY(File::Open(FL, {.filename=log, .mode="wa"}));
            TRY(log_out.write(fmt::format("{:%Y-%m-%d %H:%M:%S}: ", format_time_now())));

            p_log_out = new File(std::move(log_out));
        }

        res.body_stream.out_with_prefix(ep_out, [p_log_out](std::string_view sv) -> Result<void> {
            if (p_log_out) TRY(p_log_out->write(sv));
            return Ok();
        });

        if (output.size() >= 8 && output.substr(0, 8) == "stdio://" and not isn_lf) {
            fmt::println("");
        }

        if (p_log_out and not isn_lf) p_log_out->write("\n");
        delete p_log_out;

        return Ok();
    } else {
        auto ep_out = TRY(Endpoint::Open(FL, "stdio://"));
        ep_out << res.body << res.body_stream;
        fmt::println("");

        return Err(Error{res.status, res.status_string});
    }
}

static const char* const RESET   = "\033[0m";   // Reset to default
static const char* const BOLD    = "\033[1m";   // Bold text
static const char* const RED     = "\033[31m";  // Red text
static const char* const GREEN   = "\033[32m";  // Green text
static const char* const YELLOW  = "\033[33m";  // Yellow text
static const char* const BLUE    = "\033[34m";  // Blue
static constexpr char FORMAT[] = "{}{:%Y-%m-%d %H:%M:%S} {}{}{}:{} {}";

void delameta::info(const char*, int, const std::string& msg) {
    if (Opts::verbose)
        fmt::println(FORMAT, BLUE, format_time_now(), BOLD, GREEN, "info", RESET, msg);
}
void delameta::warning(const char*, int, const std::string& msg) {
    fmt::println(FORMAT, BLUE, format_time_now(), BOLD, YELLOW, "warning", RESET, msg);
}
void delameta::panic(const char*, int, const std::string& msg) {
    fmt::println(FORMAT, BLUE, format_time_now(), BOLD, RED, "panic", RESET, msg);
    exit(1);
}
