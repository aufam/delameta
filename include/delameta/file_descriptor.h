#ifndef PROJECT_DELAMETA_FILE_DESCRIPTOR_H
#define PROJECT_DELAMETA_FILE_DESCRIPTOR_H

#include "delameta/stream.h"

namespace Project::delameta {
    
    class FileDescriptor : public Descriptor {
    protected:
        FileDescriptor(const char* file, int line, int fd);

    public:
        FileDescriptor(FileDescriptor&&);
        virtual ~FileDescriptor();

        static Result<FileDescriptor> Open(const char* file, int line, const char* __file, int __oflag);

        Result<std::vector<uint8_t>> read() override;
        Stream read_as_stream(size_t n) override;
        Result<std::vector<uint8_t>> read_until(size_t n) override;

        Result<void> write(std::string_view data) override;
        using Descriptor::write;

        Result<size_t> file_size();

        FileDescriptor& operator<<(Stream& s);
        FileDescriptor& operator>>(Stream& s);

        int fd;
        int timeout;
        const char* file;
        int line;
    };
}

#endif
