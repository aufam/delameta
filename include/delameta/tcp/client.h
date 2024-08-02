#ifndef PROJECT_DELAMETA_TCP_CLIENT_H
#define PROJECT_DELAMETA_TCP_CLIENT_H

#include "delameta/socket/stream.h"

namespace Project::delameta::tcp {

    class Client : public delameta::Movable {
    public:
        Client(Client&&);
        Client& operator=(Client&&);

        struct Args {
            std::string host; 
            int port; 
            int timeout = 5;
        };

        static Result<Client> New(const char* file, int line, Args args);
        virtual ~Client();

        Result<std::vector<uint8_t>> request(delameta::Stream in_stream);

    protected:
        explicit Client(socket::Stream* stream);
        socket::Stream* stream;
    };
}

#endif
