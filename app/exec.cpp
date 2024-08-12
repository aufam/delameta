#include <boost/preprocessor.hpp>
#include "delameta/http/server.h"
#include <cstdio>
#include <memory>
#include <iostream>

using namespace Project;
using namespace delameta;
using http::Server;
using etl::Err;
using etl::Ok;

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/exec", ("GET")), 
    (execute_cmd), (std::string, cmd, http::arg::arg("cmd")),
    (Server::Result<std::string>)
) {
    char buffer[128];
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(::popen(cmd.c_str(), "r"), ::pclose);

    if (!pipe) {
        return Err(Server::Error{http::StatusInternalServerError, "Unable to execute " + cmd});
    }

    while (::fgets(buffer, 128, pipe.get()) != nullptr) {
        result += buffer;
    }
    
    return Ok(std::move(result));
}
