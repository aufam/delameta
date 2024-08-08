#ifndef PROJECT_DELAMETA_TCP_SERVER_H
#define PROJECT_DELAMETA_TCP_SERVER_H

#include "delameta/socket.h"

namespace Project::delameta::tcp {

    class Server : public StreamSessionServer {
    public:
        Server(Server&&);
        Server& operator=(Server&&);

        struct Args {
            std::string host;
            int max_socket = 4;
        };

        static Result<Server> New(const char* file, int line, Args args);
        virtual ~Server();

        Result<void> start();
        void stop();

    protected:
        explicit Server(Socket* socket);
        Socket* socket;
        std::function<void()> on_stop;
    };
}

#endif
