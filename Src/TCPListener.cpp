#include "TCPListener.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

TCPListener::TCPListener() : SocketBase() {}

TCPListener::TCPListener(TCPListener&& other) noexcept
    : SocketBase(std::move(other)) {}

TCPListener& TCPListener::operator=(TCPListener&& other) noexcept {
    SocketBase::operator=(std::move(other));
    return *this;
}

bool TCPListener::bind_and_listen(int port, int backlog) {
    if (!is_valid() && !create_socket()) {
        return false;
    }

    // Allow immediate port reuse after server restart (avoids TIME_WAIT bind errors)
    int opt = 1;
    if (::setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close_socket();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all network interfaces (0.0.0.0)
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(sock_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket();
        return false;
    }

    if (::listen(sock_fd, backlog) < 0) {
        close_socket();
        return false;
    }

    return true;
}

TCPStream TCPListener::accept_connection() {
    if (!is_valid()) {
        return TCPStream(-1);
    }

    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = ::accept(sock_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
        return TCPStream(-1); // Caller checks stream.is_valid()
    }

    return TCPStream(client_fd);
}