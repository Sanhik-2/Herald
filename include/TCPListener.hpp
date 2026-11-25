#pragma once

#include "socket_base.hpp"
#include "TCPStream.hpp"

class TCPListener : public SocketBase {
public:
    TCPListener();
    TCPListener(TCPListener&& other) noexcept;
    TCPListener& operator=(TCPListener&& other) noexcept;

    bool bind_and_listen(int port, int backlog = 128);
    TCPStream accept_connection();
};