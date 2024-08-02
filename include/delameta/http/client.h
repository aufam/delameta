#ifndef PROJECT_HTTP_CLIENT_H
#define PROJECT_HTTP_CLIENT_H

#include "delameta/tcp/client.h"
#include "delameta/http/request.h"
#include "delameta/http/response.h"

namespace Project::delameta::http {

    class Client : public tcp::Client {
    public:
        Client(Client&&) = default;
        Client& operator=(Client&&) = default;
        virtual ~Client() = default;

        static Result<Client> New(const char* file, int line, Args args);
        Result<ResponseReader> request(RequestWriter req);

    protected:
        explicit Client(tcp::Client&&);
        using tcp::Client::Client;
    };
}

#endif