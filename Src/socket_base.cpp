
#include "socket_base.hpp"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

SocketBase::SocketBase() : sock_fd(-1) {}
SocketBase::SocketBase(int fd) : sock_fd(fd) {}
SocketBase::~SocketBase() {
    close_socket();
}
SocketBase::SocketBase(SocketBase&& other) noexcept : sock_fd(other.sock_fd) {
    other.sock_fd = -1;
}
SocketBase& SocketBase::operator=(SocketBase&& other) noexcept {
    if (this != &other) {
        close_socket();
        sock_fd = other.sock_fd;
        other.sock_fd = -1;
    }
    return *this;
}
bool SocketBase::create_socket(){
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    return sock_fd >= 0;
}
void SocketBase::close_socket() {
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
}
int SocketBase::get_fd() const {
    return sock_fd;
}
bool SocketBase::is_valid() const {
    return sock_fd >= 0;
}
