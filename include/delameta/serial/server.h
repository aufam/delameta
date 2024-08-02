#ifndef PROJECT_DELAMETA_SERIAL_SERIAL_H
#define PROJECT_DELAMETA_SERIAL_SERIAL_H

#include "delameta/file_descriptor/stream.h"

namespace Project::delameta::serial {

    class Server : delameta::Movable {
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
        explicit Server(file_descriptor::Stream* stream);
        file_descriptor::Stream* stream;
        
        using StreamSessionHandler = std::function<void(
            file_descriptor::Stream& stream, 
            const std::vector<uint8_t>& data
        )>;

        StreamSessionHandler handler;
        std::function<void()> on_stop;

        virtual void execute_stream_session(
            file_descriptor::Stream& stream, 
            const std::vector<uint8_t>& data
        );
    };
}

#endif
