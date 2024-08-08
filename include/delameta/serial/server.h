#ifndef PROJECT_DELAMETA_SERIAL_SERIAL_H
#define PROJECT_DELAMETA_SERIAL_SERIAL_H

#include "delameta/file_descriptor.h"

namespace Project::delameta::serial {

    class Server : public StreamSessionServer {
    public:
        Server(Server&&);
        Server& operator=(Server&&);

        struct Args {
            std::string port; 
            int baud; 
        };

        static Result<Server> New(const char* file, int line, Args args);
        virtual ~Server();

        Result<void> start();
        void stop();

    protected:
        explicit Server(FileDescriptor* fd);
        FileDescriptor* fd;
        std::function<void()> on_stop;
    };
}

#endif
