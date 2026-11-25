#include "PeerManager.hpp"

#include <fstream>
#include <sstream>
#include <utility>

PeerManager::PeerManager(std::string db_path) : db_path_(std::move(db_path)) {
    load_from_disk();
}

void PeerManager::upsert_peer(const std::string& name, const std::string& ip, int port) {
    std::lock_guard<std::mutex> lock(mtx_);
    registry_[name] = PeerInfo{name, ip, port, false};
    save_unlocked();
}

void PeerManager::attach_stream(const std::string& name, TCPStream&& stream) {
    std::lock_guard<std::mutex> lock(mtx_);
    streams_[name] = std::move(stream);
}

void PeerManager::disconnect_peer(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx_);
    streams_.erase(name);
}

bool PeerManager::get_peer(const std::string& name, PeerInfo& out_info) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = registry_.find(name);
    if (it == registry_.end()) {
        return false;
    }
    out_info = it->second;
    auto s_it = streams_.find(name);
    out_info.connected = (s_it != streams_.end() && s_it->second.is_valid());
    return true;
}

std::vector<PeerInfo> PeerManager::list_peers() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<PeerInfo> result;
    result.reserve(registry_.size());

    for (const auto& [name, info] : registry_) {
        PeerInfo copy = info;
        auto s_it = streams_.find(name);
        copy.connected = (s_it != streams_.end() && s_it->second.is_valid());
        result.push_back(std::move(copy));
    }
    return result;
}

void PeerManager::load_from_disk() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ifstream file(db_path_);
    if (!file.is_open()) {
        return;
    }

    registry_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string name, ip;
        int port = 0;
        if (iss >> name >> ip >> port) {
            registry_[name] = PeerInfo{name, ip, port, false};
        }
    }
}

void PeerManager::save_to_disk() const {
    std::lock_guard<std::mutex> lock(mtx_);
    save_unlocked();
}

void PeerManager::save_unlocked() const {
    std::ofstream file(db_path_, std::ios::trunc);
    if (!file.is_open()) {
        return;
    }
    for (const auto& [name, info] : registry_) {
        file << info.name << ' ' << info.ip << ' ' << info.port << '\n';
    }
}