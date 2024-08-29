#ifndef PROJECT_DELAMETA_TCP_H
#define PROJECT_DELAMETA_TCP_H

#include "delameta/stream.h"

namespace Project::delameta {
    
    class TCP : public Descriptor, public StreamSessionClient {
    protected:
        friend class Server<TCP>;
        TCP(const char* file, int line, int socket, int timeout);

    public:
        TCP(TCP&&);
        virtual ~TCP();

        struct Args {
            std::string host;
            int timeout = -1;
            int connection_timeout = 5;
        };

        static Result<TCP> Open(const char* file, int line, Args args);
        static Result<TCP> Open(Args args) { return Open("", 0, args); }

        Result<std::vector<uint8_t>> read() override;
        Result<std::vector<uint8_t>> read_until(size_t n) override;
        Stream read_as_stream(size_t n) override;

        Result<void> write(std::string_view data) override;
        using Descriptor::write;

        int socket;
        bool keep_alive;
        int timeout;
        int max;
    
        const char* file;
        int line;
    };

    template<>
    class Server<TCP> : public StreamSessionServer {
    public:
        struct Args {
            std::string host;
            int max_socket = 4;
            bool keep_alive = true;
        };

        Result<void> start(const char* file, int line, Args args);
        Result<void> start(Args args) { return start("", 0, args); }
        void stop();
    
    protected:
        std::function<void()> on_stop;
    };
}

#endif
