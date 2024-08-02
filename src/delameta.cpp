#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <iostream>
#include "delameta/stream.h"

namespace Project::delameta {
    __attribute__((weak)) 
    void info(const char*, int, const std::string&) {}

    __attribute__((weak)) 
    void warning(const char*, int, const std::string&) {}

    __attribute__((weak)) 
    void panic(const char* file, int line, const std::string& msg) { 
        std::cerr << file << ":" << std::to_string(line) << ": " << msg << '\n';
        exit(1); 
    }
}

using namespace Project::delameta;

bool delameta_detail_is_fd_alive(int fd) {
    return ::fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

int delameta_detail_set_non_blocking(int socket) {
    return ::fcntl(socket, F_SETFL, ::fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
}

bool delameta_detail_is_socket_alive(int socket) {
    uint8_t dummy;
    auto res = ::recv(socket, &dummy, 1, MSG_PEEK);
    if (res == 0)
        return false; // connection close
    if (res > 0)
        return true; // data available, socket is alive
    if (res < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
        return true; // no data available, socket is alive
    return false;
}

auto delameta_detail_get_ip(int socket) -> std::string {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    // Get the address of the remote end
    if (getpeername(socket, (sockaddr*)&addr, &addr_len) != 0) {
        panic(__FILE__, __LINE__, "Failed to get socket peername");
    }
    
    // Convert to human-readable format
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str)) == NULL) {
        panic(__FILE__, __LINE__, "Failed to get client ip");
    }
    
    return std::string(ip_str);
}