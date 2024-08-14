#ifndef PROJECT_DELAMETA_H
#define PROJECT_DELAMETA_H

#include "delameta/debug.h"

int delameta_detail_is_fd_alive(int fd);
int delameta_detail_set_non_blocking(int socket);
bool delameta_detail_is_socket_alive(int socket);
auto delameta_detail_get_ip(int socket) -> std::string;
auto delameta_detail_get_filename(int fd) -> std::string;

#endif