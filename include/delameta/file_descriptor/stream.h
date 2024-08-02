#ifndef PROJECT_DELAMETA_FILE_DESCRIPTOR_STREAM_H
#define PROJECT_DELAMETA_FILE_DESCRIPTOR_STREAM_H

#include "delameta/error.h"
#include "delameta/stream.h"

namespace Project::delameta::file_descriptor {
    
    class Stream : public delameta::Stream {
        Stream(const char* file, int line, int fd);

    public:
        Stream(Stream&&);
        virtual ~Stream();

        static Result<Stream> Open(const char* file, int line, const char* __file, int __oflag);

        Result<void> write();
        Result<std::vector<uint8_t>> read();
        Result<std::vector<uint8_t>> read_until(size_t n);
        Result<size_t> file_size();

        int fd;
        int timeout;
        const char* file;
        int line;
    };
}

namespace Project::delameta {
    template <>
    Stream& Stream::operator<<(file_descriptor::Stream other);
}

#endif
