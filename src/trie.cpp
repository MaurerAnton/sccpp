#include "common.hpp"

void Trie::insert(int tokenType, const std::vector<uint8_t>& token) {
    Trie* node = this;
    for (uint8_t c : token) {
        if (node->table[c] == nullptr) {
            node->table[c] = new Trie();
        }
        node = node->table[c];
    }
    node->type = tokenType;
}

void Trie::insertClose(int tokenType, const std::vector<uint8_t>& openToken, const std::vector<uint8_t>& closeToken) {
    Trie* node = this;
    for (uint8_t c : openToken) {
        if (node->table[c] == nullptr) {
            node->table[c] = new Trie();
        }
        node = node->table[c];
    }
    node->type = tokenType;
    node->close = closeToken;
}

Trie::MatchResult Trie::match(const uint8_t* token, size_t len) const {
    const Trie* node = this;
    int depth = 0;
    const Trie* prevClosedNode = nullptr;
    int prevClosedDepth = 0;

    for (depth = 0; (size_t)depth < len; depth++) {
        uint8_t c = token[depth];
        if (node->table[c] == nullptr) break;
        node = node->table[c];
        if (!node->close.empty()) {
            prevClosedNode = node;
            prevClosedDepth = depth;
        }
    }

    if (node->close.empty() && prevClosedNode != nullptr) {
        return {prevClosedNode->type, prevClosedDepth, prevClosedNode->close};
    }
    return {node->type, depth, node->close};
}

void CheckDuplicates::add(int64_t key, const std::vector<uint8_t>& hash) {
    auto it = hashes.find(key);
    if (it != hashes.end()) {
        it->second.push_back(hash);
    } else {
        hashes[key] = {hash};
    }
}

bool CheckDuplicates::check(int64_t key, const std::vector<uint8_t>& hash) {
    auto it = hashes.find(key);
    if (it == hashes.end()) return false;
    for (const auto& h : it->second) {
        if (h == hash) return true;
    }
    return false;
}
