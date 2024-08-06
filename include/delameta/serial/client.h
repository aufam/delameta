#ifndef PROJECT_DELAMETA_TCP_CLIENT_H
#define PROJECT_DELAMETA_TCP_CLIENT_H

#include "delameta/file_descriptor.h"

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

        Result<std::vector<uint8_t>> request(Stream& in_stream);

    protected:
        explicit Client(FileDescriptor* fd);
        FileDescriptor* fd;
    };
}

#endif
