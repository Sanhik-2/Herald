**Herald**
Herald is a chat Application developed in C++ programming language that is use to send Messages and receive them. 
The current structure that's going to be will be:
[User Types] 
     │
     ▼
1. Chat (Input)      ──► Reads keystrokes / strings via std::getline
     │
     ▼
2. Send              ──► Passes raw bytes to the socket: send() / write()
     │
     ▼
3. TCP/IP Layer      ──► (OS Kernel handles this: packetizing, ACKs, retransmissions)
     │
     ▼  [Over the Network / Loopback]
     │
4. Receive           ──► Reads incoming raw bytes from socket: recv() / read()
     │
     ▼
5. Window (Display)  ──► Prints to screen / updates UI: std::cout / GUI / TUI

This will be the general filesystem. 
The general architecture will be :
Herald 
    |
    |_____include
    |            |
    |            |_____chat.hpp
    |            |_____send.hpp
    |            |_____TCP.hpp
    |            |_____receive.hpp
    |            |______window.hpp
    |
    |______src
            |
    |       |_____chat.cpp
    |       |_____send.cpp
    |       |_____TCP.cpp
    |       |_____receive.cpp
    |       |______window.cpp