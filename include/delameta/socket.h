#ifndef PROJECT_DELAMETA_SOCKET_H
#define PROJECT_DELAMETA_SOCKET_H

#include "delameta/stream.h"

namespace Project::delameta {
    
    class Socket : public Descriptor {
    protected:
        Socket(const char* file, int line, int socket);

    public:
        Socket(Socket&&);
        virtual ~Socket();

        struct Args {
            std::string host; 
            int port; 
            int max_socket = 4;
        };

        static Result<Socket> New(const char* file, int line, int __domain, int __type, int __protocol);
        static Result<Socket> Accept(const char* file, int line, int __fd, void* __addr, void* __addr_len);

        Result<std::vector<uint8_t>> read_until(size_t n);
        Result<std::vector<uint8_t>> read() override;
        Stream read_as_stream(size_t n) override;
        Result<void> write(std::string_view data) override;

        int socket;
        bool keep_alive;
        int timeout;
        int max;
    
        const char* file;
        int line;
    };
}

#endif
