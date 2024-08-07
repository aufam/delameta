#include "delameta/http/server.h"
#include <cstdio>
#include <memory>

using namespace Project;
using namespace delameta;
using http::Server;
using etl::Err;
using etl::Ok;

void exec_init(Server& app) {
    app.Get("/exec", std::tuple{http::arg::arg("cmd")},
    [](std::string cmd) -> Server::Result<std::string> {
        char buffer[128];
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(::popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            return Err(Server::Error{http::StatusInternalServerError, "Unable to execute " + cmd});
        }
        while (::fgets(buffer, 128, pipe.get()) != nullptr) {
            result += buffer;
        }
        return Ok(std::move(result));
    });
}


