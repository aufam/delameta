#ifndef PROJECT_DELAMETA_H
#define PROJECT_DELAMETA_H

#include "delameta/debug.h"
#include "delameta/stream.h"

int delameta_detail_set_non_blocking(int socket);
bool delameta_detail_is_fd_alive(int fd);
bool delameta_detail_is_socket_alive(int socket);
auto delameta_detail_get_ip(int socket) -> std::string;
auto delameta_detail_get_filename(int fd) -> std::string;

auto delameta_detail_resolve_domain(
    const std::string& domain, 
    int sock_type, 
    bool for_binding
) -> Project::etl::Result<struct addrinfo*, int>;

auto delameta_detail_read(
    const char* file, int line, 
    int fd, void* ssl,
    int timeout, 
    bool(*is_alive)(int)
) -> Project::delameta::Result<std::vector<uint8_t>>;

auto delameta_detail_recvfrom(
    const char* file, int line, 
    int fd, int timeout, 
    void *peer
) -> Project::delameta::Result<std::vector<uint8_t>>;

auto delameta_detail_read_until(
    const char* file, int line, 
    int fd, void* ssl,
    int timeout, 
    bool(*is_alive)(int), size_t n
) -> Project::delameta::Result<std::vector<uint8_t>>;

auto delameta_detail_recvfrom_until(
    const char* file, int line, 
    int fd, int timeout, 
    void *peer, size_t n
) -> Project::delameta::Result<std::vector<uint8_t>>;

auto delameta_detail_read_as_stream(
    const char* file, int line, 
    int timeout, 
    Project::delameta::Descriptor* self, size_t n
) -> Project::delameta::Stream;

auto delameta_detail_write(
    const char* file, int line, 
    int fd, void* ssl,
    int timeout, 
    bool(*is_alive)(int), std::string_view data
) -> Project::delameta::Result<void>;

auto delameta_detail_sendto(
    const char* file, int line, 
    int fd, int timeout, 
    void* peer, std::string_view data
) -> Project::delameta::Result<void>;

namespace Project::delameta {

    struct LogError {
        const char* file; 
        int line;

        template <typename F>
        Error operator()(int code, F err_to_str) const {
            std::string what = err_to_str(code);
            warning(file, line, what);
            return Error{code, std::move(what)};
        }
    };
}

auto delameta_detail_create_socket(void* hint, const Project::delameta::LogError& log_error) -> Project::delameta::Result<int>;

#endif