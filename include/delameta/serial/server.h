#ifndef PROJECT_DELAMETA_SERIAL_SERIAL_H
#define PROJECT_DELAMETA_SERIAL_SERIAL_H

#include "delameta/file_descriptor.h"

namespace Project::delameta::serial {

    class Server : public StreamSessionServer {
    public:
        Server(Server&&);
        virtual ~Server();

        struct Args {
            std::string port; 
            int baud; 
        };

        static Result<Server> New(const char* file, int line, Args args);

        Result<void> start();
        void stop();

        FileDescriptor fd;

    protected:
        explicit Server(FileDescriptor&& fd);
        std::function<void()> on_stop;
    };
}

#endif
