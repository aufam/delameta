#ifndef PROJECT_DELAMETA_TCP_SERVER_H
#define PROJECT_DELAMETA_TCP_SERVER_H

#include "delameta/socket.h"

namespace Project::delameta::tcp {

    class Server : public StreamSessionServer {
    public:
        Server(Server&&);
        virtual ~Server();

        struct Args {
            std::string host;
            int max_socket = 4;
        };

        static Result<Server> New(const char* file, int line, Args args);

        Result<void> start();
        void stop();

        Socket socket;
        int port;

    protected:
        Server(Socket&& socket, int port);
        std::function<void()> on_stop;
    };
}

#endif
