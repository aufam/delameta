// TODO

#include "delameta/tls.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
using etl::defer;

#define NOT_IMPLEMENTED return Err(Error{-1, "no impl"});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

auto TLS::Open(const char* file, int line, Args args) -> Result<TLS> {
    NOT_IMPLEMENTED
}

TLS::TLS(TCP&& tcp, void* ssl)
    : TCP(std::move(tcp)) 
    , ssl(ssl) {}

TLS::TLS(const char* file, int line, int socket, int timeout, void* ssl)
    : TCP(file, line, socket, timeout)
    , ssl(ssl) {}

TLS::TLS(TLS&& other)
    : TCP(std::move(other)) 
    , ssl(std::exchange(other.ssl, nullptr)) {}

TLS::~TLS() {}

auto TLS::read() -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto TLS::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    NOT_IMPLEMENTED
}

auto TLS::read_as_stream(size_t n) -> Stream {
    return {};
}

auto TLS::write(std::string_view data) -> Result<void> {
    NOT_IMPLEMENTED
}

auto Server<TLS>::start(const char* file, int line, Args args) -> Result<void> {
    NOT_IMPLEMENTED
}

void Server<TLS>::stop() {}

#pragma GCC diagnostic pop