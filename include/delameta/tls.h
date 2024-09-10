#ifndef PROJECT_DELAMETA_TLS_H
#define PROJECT_DELAMETA_TLS_H

#include "delameta/tcp.h"

namespace Project::delameta {
    
    class TLS : public TCP {
    protected:
        friend class Server<TLS>;
        TLS(TCP&& tcp, void* ssl);
        TLS(const char* file, int line, int socket, int timeout, void* ssl);

    public:
        TLS(TLS&&);
        virtual ~TLS();

        static Result<TLS> Open(const char* file, int line, Args args);
        static Result<TLS> Open(Args args) { return Open("", 0, args); }

        Result<std::vector<uint8_t>> read() override;
        Result<std::vector<uint8_t>> read_until(size_t n) override;
        Stream read_as_stream(size_t n) override;

        Result<void> write(std::string_view data) override;
        using Descriptor::write;

        void* ssl;
    };

    template<>
    class Server<TLS> : public StreamSessionServer {
    public:
        struct Args {
            std::string host;
            std::string cert_file;
            std::string key_file;
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
