#pragma once

#include <cstdint>
#include <string>

enum class PacketType : uint8_t {
    HELLO    = 0x01,
    CHAT_MSG = 0x02
};

namespace WireProtocol {
    // Packs [type + body] -> Compress -> Encrypt -> ready for TCPStream::send_packet
    std::string encode_packet(PacketType type, const std::string& body, const std::string& password);

    // Decrypt -> Decompress -> splits [type, body]
    bool decode_packet(const std::string& wire_packet,
                       const std::string& password,
                       PacketType& out_type,
                       std::string& out_body);
}