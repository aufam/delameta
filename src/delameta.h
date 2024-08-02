#ifndef PROJECT_DELAMETA_H
#define PROJECT_DELAMETA_H

#include <string>

int delameta_detail_is_fd_alive(int fd);
int delameta_detail_set_non_blocking(int socket);
bool delameta_detail_is_socket_alive(int socket);
auto delameta_detail_get_ip(int socket) -> std::string;

namespace Project::delameta {

    __attribute__((weak)) 
    void info(const char*, int, const std::string&);

    __attribute__((weak)) 
    void warning(const char*, int, const std::string&);

    __attribute__((weak)) 
    void panic(const char* file, int line, const std::string& msg);
}

#endif