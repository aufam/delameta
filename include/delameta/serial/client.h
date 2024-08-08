#ifndef PROJECT_DELAMETA_TCP_CLIENT_H
#define PROJECT_DELAMETA_TCP_CLIENT_H

#include "delameta/file_descriptor.h"

namespace Project::delameta::serial {

    class Client : public StreamSessionClient {
    public:
        Client(Client&&) = default;
        Client& operator=(Client&&) = default;

        struct Args {
            std::string port; 
            int baud; 
            int timeout = 5;
        };

        static Result<Client> New(const char* file, int line, Args args);
        virtual ~Client() = default;
    
    protected:
        explicit Client(FileDescriptor* fd);
    };
}

#endif
