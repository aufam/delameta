#include "delameta/http/response.h"
#include "delameta/http/server.h"
#include <chrono>
#include <jwt-cpp/jwt.h>

using namespace Project;
using namespace delameta;
using namespace http;
using etl::Err;
using etl::Ok;


static const char SECRET[] = "secret";
static const char PASSWORD[] = "1407";

auto get_jwt_username(const RequestReader& req, ResponseWriter&) -> Server::Result<std::string> { 
    std::string_view token = "";
    std::string_view prefix = "Bearer ";
    auto err_unauthorized = [](std::string what) {
        return Err(Server::Error{StatusUnauthorized, std::move(what)});
    };

    auto it = req.headers.find("Authentication");
    if (it == req.headers.end()) {
        it = req.headers.find("authentication");
    } 
    if (it != req.headers.end()) {
        token = it->second;
    } else {
        return err_unauthorized("No authentication provided");
    }
    if (token.substr(0, prefix.length()) != prefix) {
        return err_unauthorized("Token doesn't starts with Bearer");
    }

    token = token.substr(prefix.length());
    
    try {
        auto decoded = jwt::decode(std::string(token));
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{SECRET})
            .with_issuer("auth-server");

        verifier.verify(decoded);

        auto claim = decoded.get_payload_claim("username");
        return Ok(claim.as_string());
    } catch (const std::exception& e) {
        return err_unauthorized(std::string("Verification failed: ") + e.what());
    }
}

void jwt_init(Server& app) {
    app.Post("/login", std::tuple{arg::json_item("username"), arg::json_item("password")},
    [](std::string username, std::string password) -> Server::Result<std::map<std::string, std::string>> {
        if (password != PASSWORD) {
            return Err(Server::Error{StatusBadRequest, "Invalid password"});
        } 

        auto token = jwt::create()
            .set_issuer("auth-server")
            .set_type("JWT")
            .set_audience("audience")
            .set_subject("user_id")
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_in(std::chrono::seconds{3600})
            .set_payload_claim("username", jwt::claim(username))
            .sign(jwt::algorithm::hs256{SECRET});

        return Ok(std::map<std::string, std::string> {
            {"username", username},
            {"access_token", std::move(token)},
        });
    });

    app.Get("/username", std::tuple{arg::depends(get_jwt_username)}, 
    [](std::string username) {
        return username;
    });
}
