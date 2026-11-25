#pragma once

#include "TCPStream.hpp"
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct PeerInfo {
    std::string name;
    std::string ip;
    int port = 0;
    bool connected = false;
};

class PeerManager {
public:
    explicit PeerManager(std::string db_path = "peers.db");

    // Add or update a peer's IP/port in the dictionary and persist to disk
    void upsert_peer(const std::string& name, const std::string& ip, int port);

    // Register an already-connected incoming or outgoing TCPStream under `name`
    void attach_stream(const std::string& name, TCPStream&& stream);

    // Mark a peer disconnected and close/remove its cached stream
    void disconnect_peer(const std::string& name);

    // Look up peer info by name (returns false if unknown)
    bool get_peer(const std::string& name, PeerInfo& out_info) const;

    // Snapshot of all known peers (with live `connected` status)
    std::vector<PeerInfo> list_peers() const;

    // Send an already-framed wire packet to `name`, dialing & caching if not yet connected.
    // `on_connect_handshake` runs right after a new TCP connection opens (e.g., to send HELLO).
    template <typename HandshakeFn>
    bool send_to_peer(const std::string& name,
                      const std::string& wire_packet,
                      HandshakeFn&& on_connect_handshake);

    // Disk persistence
    void load_from_disk();
    void save_to_disk() const;

private:
    std::string db_path_;
    mutable std::mutex mtx_;
    std::unordered_map<std::string, PeerInfo> registry_;
    std::unordered_map<std::string, TCPStream> streams_;

    void save_unlocked() const;
};

// Template definition in header so any handshake callable compiles cleanly
template <typename HandshakeFn>
bool PeerManager::send_to_peer(const std::string& name,
                               const std::string& wire_packet,
                               HandshakeFn&& on_connect_handshake) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto reg_it = registry_.find(name);
    if (reg_it == registry_.end()) {
        return false; // Unknown peer name
    }

    // 1. Check if we already have a live cached socket for this peer
    auto stream_it = streams_.find(name);
    if (stream_it != streams_.end() && stream_it->second.is_valid()) {
        if (stream_it->second.send_packet(wire_packet)) {
            return true;
        }
        // Stale/broken pipe: drop old socket and fall through to reconnect
        streams_.erase(stream_it);
    }

    // 2. Dial peer's saved IP:port
    TCPStream new_stream;
    if (!new_stream.establish_connection(reg_it->second.ip, reg_it->second.port)) {
        return false;
    }

    // 3. Run initial handshake (e.g., send encrypted HELLO packet) before sending payload
    if (!on_connect_handshake(new_stream)) {
        return false;
    }

    if (!new_stream.send_packet(wire_packet)) {
        return false;
    }

    streams_[name] = std::move(new_stream);
    return true;
}