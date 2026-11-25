#pragma once

#include <string>

class Compressor {
public:
    // Compresses raw data using Huffman coding (falls back to raw framing if compression doesn't shrink payload)
    static std::string compress(const std::string& raw);

    // Decompresses a payload produced by Compressor::compress
    static std::string decompress(const std::string& compressed);
};