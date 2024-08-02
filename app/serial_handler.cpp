#include "delameta/http/server.h"
#include "delameta/serial/client.h"

using namespace Project;
using namespace delameta::http;
using delameta::serial::Client;
using delameta::Error;
using etl::Ref;

void serial_handler_init(Server& app) {
    app.Get("/serial", std::tuple{
        arg::default_val("port", std::string("auto")), 
        arg::default_val("baud", 9600), 
        arg::default_val("timeout", 5),
        arg::arg("data"),
        arg::response
    },
    [](std::string port, int baud, int timeout, std::string_view data, Ref<ResponseWriter> res) {
        return Client::New(__FILE__, __LINE__, {port, baud, timeout}).and_then([&](Client cli) {
            delameta::Stream s;
            s << data;
            return cli.request(std::move(s));
        }).then([&](std::vector<uint8_t> data) {
            res->headers["Content-Type"] = "application/octet-stream";
            res->body = std::string(data.begin(), data.end());
        }).except([](Error err) {
            return Server::Error{StatusInternalServerError, err.what};
        });
    });
}