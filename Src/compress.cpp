#include "compress.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <queue>
#include <stdexcept>
#include <vector>
#include <arpa/inet.h> // htonl, ntohl

namespace {
    constexpr uint8_t FLAG_RAW        = 0x00;
    constexpr uint8_t FLAG_COMPRESSED = 0x01;

    struct Node {
        uint8_t ch = 0;
        uint32_t freq = 0;
        int left = -1;
        int right = -1;

        bool is_leaf() const { return left == -1 && right == -1; }
    };

    struct Code {
        uint32_t bits = 0;
        uint8_t length = 0;
    };

    int build_tree(const std::array<uint32_t, 256>& freq, std::vector<Node>& tree) {
        tree.clear();
        tree.reserve(511);

        using MinHeapEntry = std::pair<uint32_t, int>;
        std::priority_queue<MinHeapEntry, std::vector<MinHeapEntry>, std::greater<MinHeapEntry>> pq;

        for (int i = 0; i < 256; ++i) {
            if (freq[i] > 0) {
                int idx = static_cast<int>(tree.size());
                tree.push_back({static_cast<uint8_t>(i), freq[i], -1, -1});
                pq.push({freq[i], idx});
            }
        }

        if (pq.empty()) return -1;

        while (pq.size() > 1) {
            auto [freq_l, idx_l] = pq.top();
            pq.pop();

            auto [freq_r, idx_r] = pq.top();
            pq.pop();

            int parent_idx = static_cast<int>(tree.size());
            uint32_t combined_freq = freq_l + freq_r;
            tree.push_back({0, combined_freq, idx_l, idx_r});
            pq.push({combined_freq, parent_idx});
        }

        return pq.top().second;
    }

    void generate_codes_dfs(const std::vector<Node>& tree,
                            int node_idx,
                            uint32_t current_bits,
                            uint8_t depth,
                            std::array<Code, 256>& table) {
        if (node_idx == -1) return;

        const Node& node = tree[node_idx];
        if (node.is_leaf()) {
            table[node.ch] = (depth == 0) ? Code{0, 1} : Code{current_bits, depth};
            return;
        }

        generate_codes_dfs(tree, node.left,  (current_bits << 1),      depth + 1, table);
        generate_codes_dfs(tree, node.right, (current_bits << 1) | 1u, depth + 1, table);
    }

    std::array<Code, 256> generate_codes(const std::vector<Node>& tree, int root_idx) {
        std::array<Code, 256> table{};
        generate_codes_dfs(tree, root_idx, 0, 0, table);
        return table;
    }

    void append_u32_be(std::string& out, uint32_t val) {
        uint32_t net = htonl(val);
        out.append(reinterpret_cast<const char*>(&net), sizeof(net));
    }

    uint32_t read_u32_be(const char* ptr) {
        uint32_t net = 0;
        std::memcpy(&net, ptr, sizeof(net));
        return ntohl(net);
    }

    std::string wrap_raw(const std::string& raw) {
        std::string out;
        out.reserve(1 + raw.size());
        out.push_back(static_cast<char>(FLAG_RAW));
        out.append(raw);
        return out;
    }
}

std::string Compressor::compress(const std::string& raw) {
    // Very small payloads won't beat the header overhead
    if (raw.size() < 16) {
        return wrap_raw(raw);
    }

    // 1. Count byte frequencies
    std::array<uint32_t, 256> freq{};
    uint16_t unique_count = 0;
    for (unsigned char c : raw) {
        if (freq[c] == 0) {
            ++unique_count;
        }
        ++freq[c];
    }

    // 2. Build arena tree and bit-code lookup table
    std::vector<Node> tree;
    int root_idx = build_tree(freq, tree);
    auto codes = generate_codes(tree, root_idx);

    // 3. Serialize header: [FLAG_COMPRESSED (1B)] [orig_len (4B)] [unique_count (1B)] [entries (5B * K)]
    std::string out;
    out.reserve(1 + 4 + 1 + (unique_count * 5) + raw.size());
    out.push_back(static_cast<char>(FLAG_COMPRESSED));
    append_u32_be(out, static_cast<uint32_t>(raw.size()));
    out.push_back(static_cast<char>(unique_count == 256 ? 0 : unique_count));

    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            out.push_back(static_cast<char>(i));
            append_u32_be(out, freq[i]);
        }
    }

    // 4. Bit-pack the encoded stream (MSB-first)
    uint8_t bit_buffer = 0;
    uint8_t bit_count = 0;

    for (unsigned char c : raw) {
        const Code& code = codes[c];
        for (int b = code.length - 1; b >= 0; --b) {
            uint8_t bit = (code.bits >> b) & 1u;
            bit_buffer = static_cast<uint8_t>((bit_buffer << 1) | bit);
            ++bit_count;

            if (bit_count == 8) {
                out.push_back(static_cast<char>(bit_buffer));
                bit_buffer = 0;
                bit_count = 0;
            }
        }
    }

    // Flush remaining trailing bits padded with zeros on the right
    if (bit_count > 0) {
        bit_buffer = static_cast<uint8_t>(bit_buffer << (8 - bit_count));
        out.push_back(static_cast<char>(bit_buffer));
    }

    // 5. If Huffman + header is not smaller than raw + 1B flag, send raw instead
    if (out.size() >= raw.size() + 1) {
        return wrap_raw(raw);
    }

    return out;
}

std::string Compressor::decompress(const std::string& compressed) {
    if (compressed.empty()) {
        throw std::invalid_argument("Empty compressed payload");
    }

    const uint8_t flag = static_cast<uint8_t>(compressed[0]);

    // Case 1: Uncompressed raw passthrough
    if (flag == FLAG_RAW) {
        return compressed.substr(1);
    }

    if (flag != FLAG_COMPRESSED) {
        throw std::invalid_argument("Unknown compression header flag");
    }

    // Minimum header size: 1B flag + 4B orig_len + 1B unique_count
    if (compressed.size() < 6) {
        throw std::invalid_argument("Truncated compressed header");
    }

    // 1. Read original length and unique symbol count
    const uint32_t orig_len = read_u32_be(compressed.data() + 1);
    const uint8_t raw_unique = static_cast<uint8_t>(compressed[5]);
    const uint16_t unique_count = (raw_unique == 0) ? 256 : raw_unique;

    const size_t table_bytes = static_cast<size_t>(unique_count) * 5;
    const size_t header_end = 6 + table_bytes;

    if (compressed.size() < header_end) {
        throw std::invalid_argument("Truncated Huffman frequency table");
    }

    // 2. Reconstruct the exact frequency table and arena tree
    std::array<uint32_t, 256> freq{};
    size_t offset = 6;
    for (uint16_t i = 0; i < unique_count; ++i) {
        uint8_t ch = static_cast<uint8_t>(compressed[offset]);
        uint32_t count = read_u32_be(compressed.data() + offset + 1);
        if (count == 0) {
            throw std::invalid_argument("Invalid zero frequency in Huffman header");
        }
        freq[ch] = count;
        offset += 5;
    }

    std::vector<Node> tree;
    int root_idx = build_tree(freq, tree);
    if (root_idx == -1) {
        throw std::runtime_error("Failed to rebuild Huffman tree");
    }

    // 3. Decode bits until exactly `orig_len` bytes are recovered
    std::string decoded;
    decoded.reserve(orig_len);

    // Special case: single unique character tree (root is already a leaf)
    if (tree[root_idx].is_leaf()) {
        // Verify enough padding bits exist in payload for `orig_len` bits
        size_t required_bytes = (orig_len + 7) / 8;
        if (compressed.size() - header_end < required_bytes) {
            throw std::invalid_argument("Truncated single-symbol bitstream");
        }
        decoded.assign(orig_len, static_cast<char>(tree[root_idx].ch));
        return decoded;
    }

    int curr_node = root_idx;
    for (size_t byte_idx = header_end; byte_idx < compressed.size() && decoded.size() < orig_len; ++byte_idx) {
        uint8_t byte = static_cast<uint8_t>(compressed[byte_idx]);

        for (int bit_pos = 7; bit_pos >= 0 && decoded.size() < orig_len; --bit_pos) {
            uint8_t bit = (byte >> bit_pos) & 1u;
            curr_node = (bit == 0) ? tree[curr_node].left : tree[curr_node].right;

            if (curr_node < 0 || curr_node >= static_cast<int>(tree.size())) {
                throw std::runtime_error("Corrupted Huffman bitstream traversal");
            }

            if (tree[curr_node].is_leaf()) {
                decoded.push_back(static_cast<char>(tree[curr_node].ch));
                curr_node = root_idx;
            }
        }
    }

    if (decoded.size() != orig_len) {
        throw std::invalid_argument("Truncated Huffman bitstream: incomplete message");
    }

    return decoded;
}