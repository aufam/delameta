#include <boost/preprocessor.hpp>
#include <delameta/http/http.h>
#include <cstdio>

using namespace Project;
using namespace delameta;
using etl::Err;
using etl::Ok;
using etl::defer;

HTTP_EXTERN_OBJECT(app);

// execute a command line
static HTTP_ROUTE(
    ("/exec", ("GET")), 
    (execute_cmd), (std::string, cmd, http::arg::arg("cmd")),
    (http::Result<std::string>)
) {
    char buffer[128];
    std::string result;

    auto pipe = ::popen(cmd.c_str(), "r");
    auto pipe_close = defer | [&]() { ::pclose(pipe); };

    if (!pipe) {
        return Err(http::Error{http::StatusInternalServerError, "Unable to execute " + cmd});
    }

    while (::fgets(buffer, 128, pipe) != nullptr) {
        result += buffer;
    }
    
    return Ok(std::move(result));
}
