#include "TCPStream.hpp"

#include <sys/socket.h>  // connect, send, recv
#include <netinet/in.h>  // sockaddr_in, htons
#include <arpa/inet.h>   // inet_pton
#include <unistd.h>      // close
#include <vector>        // std::vector for receive buffer
#include <utility>       // std::move



TCPStream :: TCPStream() : SocketBase() {}
TCPStream :: TCPStream(int fd) : SocketBase(fd) {}
TCPStream::TCPStream(TCPStream&& other) noexcept 
    : SocketBase(std::move(other)) {}
TCPStream& TCPStream::operator=(TCPStream&& other) noexcept {
    SocketBase::operator=(std::move(other));
    return *this;
}
bool TCPStream::establish_connection(const std::string& ip, int port){
    //check if socket is valid or create a new one
    if (!is_valid() && !create_socket()) { return false;}

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        close_socket();
        return false;
    }
    if (::connect(sock_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        close_socket();
        return false;
    }
    return true;
}

ssize_t TCPStream::send_message(const std::string& msg) {
    if (!is_valid()) {
        return -1;
    }

    size_t total_sent = 0;
    const char* data = msg.data();
    size_t remaining = msg.size();

    while (total_sent < msg.size()) {
#ifdef MSG_NOSIGNAL
        ssize_t bytes_sent = ::send(sock_fd, data + total_sent, remaining, MSG_NOSIGNAL);
#else
        ssize_t bytes_sent = ::send(sock_fd, data + total_sent, remaining, 0);
#endif
        if (bytes_sent < 0) {
            return -1; // Socket error or broken pipe
        }
        total_sent += bytes_sent;
        remaining -= bytes_sent;
    }

    return static_cast<ssize_t>(total_sent);
}
ssize_t TCPStream::receive_message(std::string& out_msg, size_t buffer_size) {
    if (!is_valid()){
        return -1; // Return -1 if the socket is not valid    
        }
    std::vector<char> buffer(buffer_size);
    ssize_t bytes_received = ::recv(sock_fd, buffer.data(), buffer.size(), 0);
    if (bytes_received > 0){
        out_msg.assign(buffer.data(), bytes_received);
    }
    else{
        out_msg.clear();
    }
     return bytes_received; // Return 0 for connection closed, -1 for error

}
bool TCPStream::send_packet(const std::string& payload) {
    uint32_t net_len = htonl(static_cast<uint32_t>(payload.size()));
    std::string frame;
    frame.reserve(sizeof(net_len) + payload.size());
    frame.append(reinterpret_cast<const char*>(&net_len), sizeof(net_len));
    frame.append(payload);

    return send_message(frame) == static_cast<ssize_t>(frame.size());
}

bool TCPStream::receive_packet(std::string& out_payload) {
    out_payload.clear();
    if (!is_valid()) return false;

    // 1. Read exact 4-byte length prefix
    uint32_t net_len = 0;
    char* len_buf = reinterpret_cast<char*>(&net_len);
    size_t len_read = 0;

    while (len_read < sizeof(net_len)) {
        ssize_t n = ::recv(sock_fd, len_buf + len_read, sizeof(net_len) - len_read, 0);
        if (n <= 0) return false; // 0 = peer disconnected cleanly, < 0 = error
        len_read += static_cast<size_t>(n);
    }

    uint32_t payload_len = ntohl(net_len);
    constexpr uint32_t MAX_PACKET_SIZE = 16 * 1024 * 1024; // 16 MB sanity cap
    if (payload_len > MAX_PACKET_SIZE) {
        return false;
    }

    // 2. Read exact `payload_len` bytes
    out_payload.resize(payload_len);
    size_t total_read = 0;

    while (total_read < payload_len) {
        ssize_t n = ::recv(sock_fd, &out_payload[total_read], payload_len - total_read, 0);
        if (n <= 0) {
            out_payload.clear();
            return false;
        }
        total_read += static_cast<size_t>(n);
    }

    return true;
}

std::string TCPStream::get_peer_ip() const {
    if (!is_valid()) return "";
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getpeername(sock_fd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return "";
    }
    char ip_str[INET_ADDRSTRLEN]{};
    if (::inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str)) == nullptr) {
        return "";
    }
    return std::string(ip_str);
}

