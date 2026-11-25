
#pragma once

#include <sys/types.h>

#define SOCKET_BASE_HPP
class SocketBase {
public:
    SocketBase();
    explicit SocketBase(int fd);
    virtual ~SocketBase();

    // Sockets are non-copyable (unique resource handle)
    SocketBase(const SocketBase&) = delete;
    SocketBase& operator=(const SocketBase&) = delete;

    // Sockets are moveable
    SocketBase(SocketBase&& other) noexcept;
    SocketBase& operator=(SocketBase&& other) noexcept;

    bool create_socket();
    void close_socket();

    int get_fd() const;
    bool is_valid() const;

protected:
    int sock_fd;
};
