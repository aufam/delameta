#include <boost/preprocessor.hpp>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/file.h>
#include <delameta/opts.h>
#include <delameta/endpoint.h>
#include <delameta/utils.h>

using namespace Project;
using namespace Project::delameta;
using etl::Err;
using etl::Ok;

HTTP_DEFINE_OBJECT(app);

using Headers = std::unordered_map<std::string, std::string>;

template<>
auto Opts::convert_string_into(const std::string& str) -> Result<Headers> {
    if (str == "") return Ok(Headers{});
    return json::deserialize<Headers>(str).except([](const char* err) { return Error{-1, err}; });
}

static auto now() {
    return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
}

OPTS_MAIN(
    (Delameta, "Delameta CLI Tools")
    ,
    /*   Type   |  Arg   | Short |   Long   |              Help                |      Default   */
    (std::string, host   , 'H'   , "host"   , "Specify host and port"          , "localhost:5000")
    (int        , n_sock , 'n'   , "n-sock" , "Number of socket server"        , "4"             )
    (std::string, url_str, 'u'   , "url"    , "Specify URL for HTTP request"   , ""              )

    // for sub command
    (std::string, cmd    , 'c'   , "cmd"    , "Specify sub command (optional)" , ""              )
    (Headers    , args   , 'a'   , "args"   , "Sub command's args"             , ""              )
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

    // launch http server if cmd and url are not specified
    if (cmd == "" and url_str == "") {
        fmt::println("Server is running on {}", host);
        return app.listen(http::Http::ListenArgs{
            .host=host,
            .cert_file=cert,
            .key_file=key,
            .max_socket=n_sock,
        });
    }

    // setup request
    http::RequestWriter req;
    req.version = "HTTP/1.1";
    req.headers = std::move(args);

    // setup body
    if ((not data.empty()) + (not input.empty()) + (not file.empty()) > 1) {
        return Err(Error{-1, "multiple data source"});
    }
    if (data != "") {
        req.headers["Content-Length"] = std::to_string(data.size());
        req.body_stream << std::move(data);
    } else if (input != "") {
        auto ep_in = TRY(Endpoint::Open(FL, input));
        auto read_data = TRY(ep_in.read());
        req.headers["Content-Length"] = std::to_string(read_data.size());
        req.body_stream << std::move(read_data);
    } else if (file != "") {
        auto f = TRY(File::Open(FL, {file}));
        req.headers["Content-Length"] = std::to_string(f.file_size());
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

    // create http response
    auto res = [&]() {
        // create response using http request
        if (not url_str.empty()) {
            req.url = url_str;
            return http::request(std::move(req));
        }

        // dummy client with string stream
        class DummyClient : public StreamSessionClient {
        public:
            http::Http& http;
            StringStream ss;
            DummyClient(http::Http& http) : StreamSessionClient(ss), http(http), ss() {}

            delameta::Result<std::vector<uint8_t>> request(Stream& in_stream) override {
                in_stream >> ss;
                auto [req, res] = http.execute(ss);
                ss.flush();
                res.dump() >> ss;
                return Ok(std::vector<uint8_t>());
            }
        };
        DummyClient dummy_client(app);

        // create response using cmd from router path
        req.url = host + cmd;
        return http::request(dummy_client, std::move(req));
    }();

    if (res.is_err()) {
        return Err(std::move(res.unwrap_err()));
    }

    auto &response = res.unwrap();
    if (p_heads) {
        for (auto &[key, value]: response.headers) {
            fmt::println("{}: {}", key, value);
        }
        fmt::println("");
    }

    // return ok or error
    if (response.status < 300) {
        // output the response
        auto ep_out = TRY(Endpoint::Open(FL, output));
        File* p_log_out = nullptr;
        if (log != "") {
            auto log_out = TRY(File::Open(FL, File::Args{.filename=log, .mode="wa"}));
            TRY(log_out.write(fmt::format("{:%Y-%m-%d %H:%M:%S}: ", now())));

            p_log_out = new File(std::move(log_out));
        }

        response.body_stream.out_with_prefix(ep_out, [p_log_out](std::string_view sv) -> Result<void> {
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
        ep_out << response.body << response.body_stream;
        fmt::println("");

        return Err(Error{response.status, std::string(response.status_string)});
    }
}

void delameta::info(const char* file, int line, const std::string& msg) {
    if (not Opts::verbose or not file) return;
    fmt::print(fmt::fg(fmt::terminal_color::blue), "{:%H:%M:%S} {}:{} ", now(), file, line);
    fmt::print(fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold, "[INFO] ");
    fmt::println("{}", msg);
}

void delameta::warning(const char* file, int line, const std::string& msg) {
    if (not file) return;
    fmt::print(fmt::fg(fmt::terminal_color::blue), "{:%H:%M:%S} {}:{} ", now(), file, line);
    fmt::print(fmt::fg(fmt::terminal_color::yellow) | fmt::emphasis::bold, "[WARNING] ");
    fmt::println("{}", msg);
}

void delameta::panic(const char* file, int line, const std::string& msg) {
    if (not file) exit(1);
    fmt::print(fmt::fg(fmt::terminal_color::blue), "{:%H:%M:%S} {}:{} ", now(), file, line);
    fmt::print(fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold, "[PANIC] ");
    fmt::println("{}", msg);
    exit(1);
}
