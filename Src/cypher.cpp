#include "cypher.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>

namespace {
    constexpr size_t AES_KEY_SIZE = 32; // 256 bits
    constexpr size_t GCM_IV_SIZE  = 12; // 96 bits (standard for GCM)
    constexpr size_t GCM_TAG_SIZE = 16; // 128 bits

    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    using EvpMdCtxPtr     = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

    // Converts any password string into a deterministic 32-byte key using SHA-256
    std::vector<uint8_t> derive_key(const std::string& password) {
        std::vector<uint8_t> key(AES_KEY_SIZE);
        EvpMdCtxPtr md_ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
        if (!md_ctx) {
            throw std::runtime_error("Failed to create MD context");
        }

        unsigned int len = 0;
        if (EVP_DigestInit_ex(md_ctx.get(), EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(md_ctx.get(), password.data(), password.size()) != 1 ||
            EVP_DigestFinal_ex(md_ctx.get(), key.data(), &len) != 1) {
            throw std::runtime_error("Failed to derive key from password");
        }

        return key;
    }
}

std::string Cypher::encrypt(const std::string& plaintext, const std::string& password) {
    std::vector<uint8_t> key = derive_key(password);

    // 1. Generate a fresh 12-byte random IV for this message
    std::vector<uint8_t> iv(GCM_IV_SIZE);
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        throw std::runtime_error("Failed to generate random IV");
    }

    std::vector<uint8_t> ciphertext(plaintext.size());
    std::vector<uint8_t> tag(GCM_TAG_SIZE);
    int len = 0;
    int final_len = 0;

    // 2. Create RAII cipher context
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP cipher context");
    }

    // 3. Initialize AES-256-GCM
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1) {
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    // 4. Encrypt plaintext bytes
    if (!plaintext.empty()) {
        if (EVP_EncryptUpdate(ctx.get(),
                              ciphertext.data(),
                              &len,
                              reinterpret_cast<const uint8_t*>(plaintext.data()),
                              static_cast<int>(plaintext.size())) != 1) {
            throw std::runtime_error("EVP_EncryptUpdate failed");
        }
    }

    // 5. Finalize encryption
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &final_len) != 1) {
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }
    ciphertext.resize(len + final_len);

    // 6. Extract the 16-byte authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag.data()) != 1) {
        throw std::runtime_error("Failed to extract GCM authentication tag");
    }

    // 7. Pack everything into one binary std::string: [IV (12B)] + [Ciphertext] + [Tag (16B)]
    std::string packed;
    packed.reserve(iv.size() + ciphertext.size() + tag.size());
    packed.append(reinterpret_cast<const char*>(iv.data()), iv.size());
    packed.append(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    packed.append(reinterpret_cast<const char*>(tag.data()), tag.size());

    return packed;
}

std::string Cypher::decrypt(const std::string& packed_ciphertext, const std::string& password) {
    // Must at least contain the 12-byte IV and 16-byte Tag
    if (packed_ciphertext.size() < GCM_IV_SIZE + GCM_TAG_SIZE) {
        throw std::invalid_argument("Ciphertext payload too short");
    }

    std::vector<uint8_t> key = derive_key(password);

    // 1. Unpack [IV (12B)] + [Ciphertext] + [Tag (16B)]
    const auto* raw = reinterpret_cast<const uint8_t*>(packed_ciphertext.data());
    const uint8_t* iv_ptr = raw;
    const uint8_t* cipher_ptr = raw + GCM_IV_SIZE;
    const size_t cipher_len = packed_ciphertext.size() - GCM_IV_SIZE - GCM_TAG_SIZE;
    const uint8_t* tag_ptr = raw + GCM_IV_SIZE + cipher_len;

    std::vector<uint8_t> plaintext(cipher_len);
    int len = 0;
    int final_len = 0;

    // 2. Create RAII cipher context
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP cipher context");
    }

    // 3. Initialize decryption with the extracted IV
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), iv_ptr) != 1) {
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    // 4. Decrypt ciphertext bytes
    if (cipher_len > 0) {
        if (EVP_DecryptUpdate(ctx.get(),
                              plaintext.data(),
                              &len,
                              cipher_ptr,
                              static_cast<int>(cipher_len)) != 1) {
            throw std::runtime_error("EVP_DecryptUpdate failed");
        }
    }

    // 5. Provide the expected 16-byte authentication tag BEFORE finalizing
    // (const_cast is required by OpenSSL's C API signature, though it only reads the tag)
    if (EVP_CIPHER_CTX_ctrl(ctx.get(),
                            EVP_CTRL_GCM_SET_TAG,
                            GCM_TAG_SIZE,
                            const_cast<uint8_t*>(tag_ptr)) != 1) {
        throw std::runtime_error("Failed to set expected GCM tag");
    }

    // 6. Finalize and verify authenticity (returns <= 0 if tampered or wrong password!)
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &final_len) <= 0) {
        throw std::runtime_error("Decryption failed: message corrupted or wrong password");
    }
    plaintext.resize(len + final_len);

    return std::string(reinterpret_cast<const char*>(plaintext.data()), plaintext.size());
}