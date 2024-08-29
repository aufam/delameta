#ifndef PROJECT_DELAMETA_FILE_H
#define PROJECT_DELAMETA_FILE_H

#include "delameta/stream.h"

namespace Project::delameta {
    
    class File : public Descriptor {
    protected:
        File(const char* file, int line, int fd);

    public:
        File(File&&);
        virtual ~File();

        struct Args {
            std::string filename;
            std::string mode = "r";
        };

        static Result<File> Open(const char* file, int line, Args args);
        static Result<File> Open(Args args) { return Open("", 0, args); }

        Result<std::vector<uint8_t>> read() override;
        Stream read_as_stream(size_t n) override;
        Result<std::vector<uint8_t>> read_until(size_t n) override;

        Result<void> write(std::string_view data) override;
        using Descriptor::write;

        size_t file_size();

        File& operator<<(Stream& s);
        File& operator>>(Stream& s);

        int fd;
        const char* file;
        int line;
    };
}

#endif
