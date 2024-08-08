#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <iostream>
#include <sstream>
#include <limits.h>
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
    sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(socket, (sockaddr*)&addr, &addr_len) != 0) {
        warning(__FILE__, __LINE__, ::strerror(errno));
        return "<Invalid socket>";
    }

    char ip_str[INET6_ADDRSTRLEN];
    void* addr_ptr = nullptr;

    if (addr.ss_family == AF_INET) { // IPv4
        addr_ptr = &((sockaddr_in*)&addr)->sin_addr;
    } else if (addr.ss_family == AF_INET6) { // IPv6
        addr_ptr = &((sockaddr_in6*)&addr)->sin6_addr;
    } else {
        warning(__FILE__, __LINE__, "Unknown address family");
        return "<Invalid socket>";
    }

    if (inet_ntop(addr.ss_family, addr_ptr, ip_str, sizeof(ip_str)) == nullptr) {
        warning(__FILE__, __LINE__, ::strerror(errno));
        return "<Invalid socket>";
    }

    return std::string(ip_str);
}

auto delameta_detail_get_filename(int fd) -> std::string {
    char filename[PATH_MAX];
    ssize_t len = readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), filename, sizeof(filename) - 1);

    if (len != -1) {
        filename[len] = '\0';
        return std::string(filename);
    } else {
        warning(__FILE__, __LINE__, ::strerror(errno));
        return "<Invalid fd>";
    }
}