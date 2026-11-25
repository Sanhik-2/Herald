# Herald

Herald is a decentralized, peer-to-peer (P2P) terminal chat node built from scratch in C++17. Designed with zero heavy runtime dependencies, it communicates directly over raw POSIX TCP sockets while securing every payload through a custom Huffman compression and OpenSSL AES-256-GCM authenticated encryption pipeline.

## Key Features

* **Serverless P2P Architecture:** Every node operates simultaneously as a multi-threaded TCP listener and client—no central broker or relay server required.
* **Custom Huffman Compression:** Arena-backed Huffman coding compresses message payloads prior to encryption, keeping entropy low before ciphering.
* **Authenticated Encryption (AES-256-GCM):** Pre-shared password key derivation (SHA-256) paired with 96-bit random IVs and 128-bit GCM authentication tags to enforce confidentiality, integrity, and peer authentication.
* **Automatic Peer Discovery & Persistence:** Nodes automatically register incoming peers upon valid `HELLO` handshakes and persist known contacts locally to `peers_<node>.db`.
* **Length-Prefixed Wire Framing:** Explicit framing over raw POSIX sockets prevents TCP stream fragmentation and coalescing issues.

## Pipeline Architecture

```text
[User CLI Input]
       │
       ▼
1. Chat & PeerManager ──► Parses commands (/add, /send, /peers) & resolves peer IP:Port
       │
       ▼
2. Huffman Compressor ──► Encodes plaintext into bit-packed compressed payload
       │
       ▼
3. AES-256-GCM Cypher ──► Encrypts payload + attaches 12-byte IV & 16-byte GCM auth tag
       │
       ▼
4. Wire Framer        ──► Prepends length header & transmits over raw POSIX TCP socket
       │
       ▼  [Over LAN / Loopback]
       │
5. TCPListener        ──► Accepts connection, validates GCM tag, decrypts & decompresses
       │
       ▼
[Terminal Output]     ──► Updates peer table on HELLO or renders message to stdout

##Project Structure
Herald/
|__build/                 #includes the files to be built in the makefile. 
├── include/
│   ├── chat.hpp          # CLI loop, command dispatcher, and node orchestration
│   ├── compress.hpp      # Custom Huffman tree & bit-stream encoder/decoder
│   ├── cypher.hpp        # OpenSSL EVP AES-256-GCM encryption & key derivation
│   ├── PeerManager.hpp   # Thread-safe peer dictionary & disk persistence
│   ├── send.hpp          # Length-prefixed packet framing and transmission
│   ├── socket_base.hpp   # RAII POSIX socket descriptor wrapper
│   ├── TCPListener.hpp   # Multi-threaded incoming connection acceptor
│   └── TCPStream.hpp     # Bidirectional TCP stream read/write abstraction
├── Src/
│   ├── main.cpp          # CLI entrypoint and argument parsing
│   ├── chat.cpp
│   ├── compress.cpp
│   ├── cypher.cpp
│   ├── PeerManager.cpp
│   ├── send.cpp
│   ├── socket_base.cpp
│   ├── TCPListener.cpp
│   └── TCPStream.cpp
├── Makefile              # Parallel incremental build configuration
└── .gitignore
```
#Building

Compile the project using the root Makefile:Bashmake -j$(nproc)
To clean build artifacts:Bash make clean
UsageLaunch a node by specifying its handle, listening port, and shared network password:
Bash ./herald <node_name> <listen_port> <shared_password>
Local Two-Node ExampleTerminal 1 (Alice):
Bash./herald alice 8081 herald_secret
Terminal 2 (Bob): 
Bash./herald bob 8082 herald_secret
In Bob's terminal, add Alice and send a message:Plaintext/add alice 127.0.0.1 8081
/send alice Hello from Herald!
Alice automatically discovers Bob's address via the encrypted HELLO handshake, saves Bob to peers_alice.db, and can reply directly:Plaintext/send bob Received loud and clear.
Available CommandsCommandDescription/add <name> <ip> <port>Manually register a peer in your local contact dictionary/peersList all saved peers and their IP:Port endpoints/send <name> <message>Compress, encrypt, and transmit a message to a peer/quitShut down the node cleanly
