// TODO

#include "delameta/file.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

#define NOT_IMPLEMENTED return Err(Error{-1, "no impl"});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

auto File::Open(const char* file, int line, Args args) -> Result<File> {
    NOT_IMPLEMENTED
}

File::File(const char* file, int line, int fd) 
    : Descriptor()
    , fd(fd)
    , file(file)
    , line(line) {}

File::File(File&& other) 
    : Descriptor()
    , fd(std::exchange(other.fd, -1))
    , file(other.file)
    , line(other.line) {}

File::~File() {}

auto File::read() -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto File::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto File::read_as_stream(size_t n) -> Stream {
    return {};
}

auto File::write(std::string_view data) -> Result<void> {
    NOT_IMPLEMENTED
}

auto File::file_size() -> size_t {
    return 0;
}

auto File::operator<<(Stream& other) -> File& {
    return *this;
}

auto File::operator>>(Stream& s) -> File& {
    return *this;
}

#pragma GCC diagnostic pop