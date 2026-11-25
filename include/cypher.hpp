#pragma once

#include <string>

class Cypher {
public:
    // Encrypts plaintext using a password -> returns [12-byte IV][Ciphertext][16-byte Tag]
    static std::string encrypt(const std::string& plaintext, const std::string& password);

    // Decrypts [12-byte IV][Ciphertext][16-byte Tag] -> throws if tampered or wrong password
    static std::string decrypt(const std::string& packed_ciphertext, const std::string& password);
};