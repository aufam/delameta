#ifndef PROJECT_DELAMETA_SOCKET_STREAM_H
#define PROJECT_DELAMETA_SOCKET_STREAM_H

#include "delameta/error.h"
#include "delameta/stream.h"

namespace Project::delameta::socket {
    
    class Stream : public delameta::Stream {
        Stream(const char* file, int line, int socket);

    public:
        Stream(Stream&&);
        virtual ~Stream();

        struct Args {
            std::string host; 
            int port; 
            int max_socket = 4;
        };

        static Result<Stream> New(const char* file, int line, int __domain, int __type, int __protocol);
        static Result<Stream> Accept(const char* file, int line, int __fd, void* __addr, void* __addr_len);

        Result<std::vector<uint8_t>> receive();
        Result<std::vector<uint8_t>> receive_until(size_t n);
        Result<void> send();

        int socket;
        bool keep_alive;
        int timeout;
        int max;
    
        const char* file;
        int line;
    };
}

#endif
