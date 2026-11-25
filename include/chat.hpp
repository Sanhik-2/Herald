#pragma once

#include "PeerManager.hpp"
#include "TCPListener.hpp"
#include "TCPStream.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Chat {
public:
    Chat(std::string my_name, int listen_port, std::string password, const std::string& db_path = "peers.db");
    ~Chat();

    // Starts the listener thread and runs the interactive CLI loop until /quit
    void run();

private:
    std::string my_name_;
    int listen_port_;
    std::string password_;

    PeerManager peers_;
    TCPListener listener_;
    std::atomic<bool> running_{false};

    std::thread accept_thread_;
    std::mutex workers_mtx_;
    std::vector<std::thread> worker_threads_;
    std::mutex console_mtx_;

    void accept_loop();
    void handle_incoming_connection(TCPStream stream);
    void monitor_outbound_stream(const std::string& peer_name, TCPStream stream);

    std::string build_hello_wire() const;
    void print_line(const std::string& msg);
    void print_help();
};