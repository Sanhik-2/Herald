// Made some changes to the file to make it more efficient and readable.
/*
Now we have client.hpp which is the header file for the client class that is used to handle the client side of the Herald application.
And it's counterpart server.hpp which is the header file for the server class that is used to handle the server side of the Herald application.
*/
#pragma once

#include "socket_base.hpp"
#include <string>

class TCPStream : public SocketBase
{
public:
    TCPStream();
    explicit TCPStream(int fd);
    ~TCPStream() override = default;

    TCPStream(TCPStream &&other) noexcept;
    TCPStream &operator=(TCPStream &&other) noexcept;

    bool establish_connection(const std::string &ip, int port);
    ssize_t send_message(const std::string &msg);
    ssize_t receive_message(std::string &out_msg, size_t buffer_size = 4096);
    bool send_packet(const std::string &payload);
    bool receive_packet(std::string &out_payload);
    std::string get_peer_ip() const;
};