#ifndef PROJECT_DELAMETA_TCP_CLIENT_H
#define PROJECT_DELAMETA_TCP_CLIENT_H

#include "delameta/file_descriptor/stream.h"

namespace Project::delameta::serial {

    class Client : public delameta::Movable {
    public:
        Client(Client&&);
        Client& operator=(Client&&);

        struct Args {
            std::string port; 
            int baud; 
            int timeout = 5;
        };

        static Result<Client> New(const char* file, int line, Args args);
        virtual ~Client();

        Result<std::vector<uint8_t>> request(delameta::Stream in_stream);

    protected:
        explicit Client(file_descriptor::Stream* stream);
        file_descriptor::Stream* stream;
    };
}

#endif
