#ifndef PROJECT_DELAMETA_SERIAL_H
#define PROJECT_DELAMETA_SERIAL_H

#include "delameta/stream.h"

namespace Project::delameta {
    
    class Serial : public Descriptor, public StreamSessionClient {
    protected:
        Serial(const char* file, int line, int fd, int timeout);

    public:
        Serial(Serial&&);
        virtual ~Serial();

        struct Args {
            std::string port;
            int baud = 9600;
            int timeout = -1;
        };

        static Result<Serial> Open(const char* file, int line, Args args);
        static Result<Serial> Open(Args args) { return Open("", 0, args); }

        Result<std::vector<uint8_t>> read() override;
        Stream read_as_stream(size_t n) override;
        Result<std::vector<uint8_t>> read_until(size_t n) override;

        Result<void> write(std::string_view data) override;
        using Descriptor::write;

        void wait_until_ready();

        int fd;
        int timeout;
        const char* file;
        int line;
    };

    template<>
    class Server<Serial> : public StreamSessionServer {
    public:
        Result<void> start(const char* file, int line, Serial::Args args);
        Result<void> start(Serial::Args args) { return start("", 0, args); }
        void stop();
    
    protected:
        std::function<void()> on_stop;
    };
}

#endif
