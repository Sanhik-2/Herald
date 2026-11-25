#include "chat.hpp"
#include "send.hpp"

#include <iostream>
#include <sstream>
#include <utility>

Chat::Chat(std::string my_name, int listen_port, std::string password, const std::string& db_path)
    : my_name_(std::move(my_name)),
      listen_port_(listen_port),
      password_(std::move(password)),
      peers_(db_path) {}

Chat::~Chat() {
    running_ = false;
    listener_.close_socket(); // Unblocks ::accept()

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    std::lock_guard<std::mutex> lock(workers_mtx_);
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.detach(); // Active sockets close automatically when PeerManager/streams destruct
        }
    }
}

std::string Chat::build_hello_wire() const {
    std::string hello_body = my_name_ + ":" + std::to_string(listen_port_);
    return WireProtocol::encode_packet(PacketType::HELLO, hello_body, password_);
}

void Chat::print_line(const std::string& msg) {
    std::lock_guard<std::mutex> lock(console_mtx_);
    std::cout << "\r" << msg << "\n> " << std::flush;
}

void Chat::print_help() {
    print_line(
        "Commands:\n"
        "  /add <name> <ip> <port>   Save a peer to your dictionary\n"
        "  /peers                    List all saved peers & connection status\n"
        "  /send <name> <message>    Send encrypted + compressed message to peer\n"
        "  /quit                     Exit Herald"
    );
}

void Chat::run() {
    if (!listener_.bind_and_listen(listen_port_)) {
        std::cerr << "[Error] Failed to bind and listen on port " << listen_port_ << "\n";
        return;
    }

    running_ = true;
    accept_thread_ = std::thread(&Chat::accept_loop, this);

    std::cout << "=== Herald Node [" << my_name_ << "] listening on port " << listen_port_ << " ===\n";
    print_help();

    std::string line;
    while (running_) {
        {
            std::lock_guard<std::mutex> lock(console_mtx_);
            std::cout << "> " << std::flush;
        }

        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) continue;

        if (line == "/quit") {
            running_ = false;
            break;
        } else if (line == "/help") {
            print_help();
        } else if (line == "/peers") {
            auto list = peers_.list_peers();
            if (list.empty()) {
                print_line("[Peers] No contacts saved yet. Use: /add <name> <ip> <port>");
            } else {
                std::ostringstream oss;
                oss << "[Peers]\n";
                for (const auto& p : list) {
                    oss << "  - " << p.name << " (" << p.ip << ":" << p.port << ") "
                        << (p.connected ? "[ONLINE]" : "[SAVED]") << "\n";
                }
                print_line(oss.str());
            }
        } else if (line.rfind("/add ", 0) == 0) {
            std::istringstream iss(line.substr(5));
            std::string name, ip;
            int port = 0;
            if (iss >> name >> ip >> port) {
                peers_.upsert_peer(name, ip, port);
                print_line("[Saved] " + name + " -> " + ip + ":" + std::to_string(port));
            } else {
                print_line("[Usage] /add <name> <ip> <port>");
            }
        } else if (line.rfind("/send ", 0) == 0) {
            std::istringstream iss(line.substr(6));
            std::string target_name;
            if (!(iss >> target_name)) {
                print_line("[Usage] /send <name> <message>");
                continue;
            }

            std::string message;
            std::getline(iss, message);
            if (!message.empty() && message[0] == ' ') {
                message.erase(0, 1);
            }
            if (message.empty()) {
                print_line("[Usage] /send <name> <message>");
                continue;
            }

            std::string wire_msg = WireProtocol::encode_packet(PacketType::CHAT_MSG, message, password_);
            std::string hello_wire = build_hello_wire();

            bool ok = peers_.send_to_peer(target_name, wire_msg, [&](TCPStream& s) {
                return s.send_packet(hello_wire);
            });

            if (!ok) {
                print_line("[Failed] Could not reach '" + target_name + "'. Check /peers or IP:port.");
            }
        } else {
            print_line("[Unknown command] Type /help for available commands.");
        }
    }

    listener_.close_socket();
}

void Chat::accept_loop() {
    while (running_) {
        TCPStream client = listener_.accept_connection();
        if (!client.is_valid()) {
            if (!running_) break;
            continue;
        }

        std::lock_guard<std::mutex> lock(workers_mtx_);
        worker_threads_.emplace_back(&Chat::handle_incoming_connection, this, std::move(client));
    }
}

void Chat::handle_incoming_connection(TCPStream stream) {
    std::string remote_ip = stream.get_peer_ip();
    std::string sender_name = remote_ip.empty() ? "unknown" : remote_ip;

    while (running_ && stream.is_valid()) {
        std::string wire_packet;
        if (!stream.receive_packet(wire_packet)) {
            break; // Disconnected
        }

        PacketType type;
        std::string body;
        if (!WireProtocol::decode_packet(wire_packet, password_, type, body)) {
            print_line("[Security Warning] Dropped invalid or tampered packet from " + sender_name);
            break;
        }

        if (type == PacketType::HELLO) {
            // Expected format: "<name>:<listen_port>"
            auto colon = body.rfind(':');
            if (colon != std::string::npos) {
                sender_name = body.substr(0, colon);
                int sender_port = std::stoi(body.substr(colon + 1));
                if (!remote_ip.empty() && sender_port > 0) {
                    peers_.upsert_peer(sender_name, remote_ip, sender_port);
                    print_line("[Handshake] Discovered peer '" + sender_name + "' at " +
                               remote_ip + ":" + std::to_string(sender_port));
                }
            }
        } else if (type == PacketType::CHAT_MSG) {
            print_line("[" + sender_name + "]: " + body);
        }
    }
}