#ifndef PROJECT_DELAMETA_TCP_SERVER_H
#define PROJECT_DELAMETA_TCP_SERVER_H

#include "delameta/socket/stream.h"

namespace Project::delameta::tcp {

    class Server : public delameta::Movable {
    public:
        Server(Server&&);
        Server& operator=(Server&&);

        struct Args {
            std::string host; 
            int port; 
            int max_socket = 4;
        };

        static Result<Server> New(const char* file, int line, Args args);
        virtual ~Server();

        Result<void> start();
        void stop();

    protected:
        explicit Server(socket::Stream* stream);
        socket::Stream* stream;
        
        using StreamSessionHandler = std::function<void(
            socket::Stream& stream, 
            const std::string& client_ip, 
            const std::vector<uint8_t>& data
        )>;

        StreamSessionHandler handler;
        std::function<void()> on_stop;

        virtual void execute_stream_session(
            socket::Stream& stream, 
            const std::string& client_ip, 
            const std::vector<uint8_t>& data
        );
    };
}

#endif
