#ifndef PROJECT_DELAMETA_UDP_H
#define PROJECT_DELAMETA_UDP_H

#include "delameta/stream.h"

namespace Project::delameta {
    
    class UDP : public Descriptor, public StreamSessionClient {
    protected:
        friend class Server<UDP>;
        UDP(const char* file, int line, int socket, int timeout, void* peer);

    public:
        UDP(UDP&&);
        virtual ~UDP();

        struct Args {
            std::string host;
            bool as_server = false;
            int timeout = -1;
        };

        static Result<UDP> Open(const char* file, int line, Args args);
        static Result<UDP> Open(Args args) { return Open("", 0, args); }

        Result<std::vector<uint8_t>> read() override;
        Stream read_as_stream(size_t n) override;
        Result<std::vector<uint8_t>> read_until(size_t n) override;

        Result<void> write(std::string_view data) override;
        using Descriptor::write;

        int socket;
        int timeout;
        void* peer;
        const char* file;
        int line;
    };

    template<>
    class Server<UDP> : public StreamSessionServer {
    public:
        struct Args {
            std::string host;
            int timeout = -1;
        };

        Result<void> start(const char* file, int line, Args args);
        Result<void> start(Args args) { return start("", 0, args); }
        void stop();
    
    protected:
        std::function<void()> on_stop;
    };
}

#endif
