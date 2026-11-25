#include "send.hpp"
#include "compress.hpp"
#include "cypher.hpp"

#include <exception>

namespace WireProtocol {

std::string encode_packet(PacketType type, const std::string& body, const std::string& password) {
    std::string frame;
    frame.reserve(1 + body.size());
    frame.push_back(static_cast<char>(type));
    frame.append(body);

    std::string compressed = Compressor::compress(frame);
    return Cypher::encrypt(compressed, password);
}

bool decode_packet(const std::string& wire_packet,
                   const std::string& password,
                   PacketType& out_type,
                   std::string& out_body) {
    try {
        std::string compressed = Cypher::decrypt(wire_packet, password);
        std::string frame = Compressor::decompress(compressed);
        if (frame.empty()) {
            return false;
        }

        uint8_t raw_type = static_cast<uint8_t>(frame[0]);
        if (raw_type != static_cast<uint8_t>(PacketType::HELLO) &&
            raw_type != static_cast<uint8_t>(PacketType::CHAT_MSG)) {
            return false;
        }

        out_type = static_cast<PacketType>(raw_type);
        out_body = frame.substr(1);
        return true;
    } catch (const std::exception&) {
        return false; // Tampered ciphertext, wrong password, or corrupted stream
    }
}

} // namespace WireProtocol