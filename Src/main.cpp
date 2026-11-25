#include "chat.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <node_name> <listen_port> <shared_password>\n"
                  << "Example:\n"
                  << "  Terminal 1: ./herald alice 8081 secret123\n"
                  << "  Terminal 2: ./herald bob   8082 secret123\n";
        return 1;
    }

    std::string node_name = argv[1];
    int listen_port = std::stoi(argv[2]);
    std::string password = argv[3];
    std::string db_file = "peers_" + node_name + ".db";

    Chat app(node_name, listen_port, password, db_file);
    app.run();

    return 0;
}