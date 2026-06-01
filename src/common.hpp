#ifndef SCCPP_COMMON_HPP
#define SCCPP_COMMON_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

/* Line types for callback */
enum LineType {
    LINE_BLANK = 0,
    LINE_CODE = 1,
    LINE_COMMENT = 2
};

/* Byte type classification for ContentByteType */
enum ByteType : uint8_t {
    BYTE_BLANK   = 0,
    BYTE_CODE    = 1,
    BYTE_COMMENT = 2,
    BYTE_STRING  = 3
};

/* State machine states */
enum State : int64_t {
    S_BLANK              = 1,
    S_CODE               = 2,
    S_COMMENT            = 3,
    S_COMMENT_CODE       = 4,
    S_MULTICOMMENT       = 5,
    S_MULTICOMMENT_CODE  = 6,
    S_MULTICOMMENT_BLANK = 7,
    S_STRING             = 8,
    S_DOCSTRING          = 9
};

/* Trie token types */
enum TokenType : int {
    T_STRING            = 1,
    T_SLCOMMENT         = 2,
    T_MLCOMMENT         = 3,
    T_COMPLEXITY        = 4,
    T_COMPLEXITY_POSTFIX = 5
};

/* Quote definition */
struct Quote {
    std::string start;
    std::string end;
    bool ignoreEscape = false;
    bool docString = false;
};

/* Language definition (loaded from JSON) */
struct Language {
    std::vector<std::string> lineComment;
    std::vector<std::string> complexityChecks;
    std::vector<std::string> complexityChecksPostfix;
    std::vector<std::string> complexityChecksPostfixExcludes;
    std::vector<std::string> extensions;
    std::vector<std::vector<std::string>> multiLine;
    std::vector<Quote> quotes;
    std::vector<std::string> keywords;
    std::vector<std::string> fileNames;
    std::vector<std::string> sheBangs;
    bool extensionFile = false;
    bool nestedMultiLine = false;
};

/* Trie node for fast token matching */
struct Trie {
    int type = 0;
    std::vector<uint8_t> close;
    Trie* table[256] = {};

    Trie() = default;
    ~Trie() {
        for (int i = 0; i < 256; i++) delete table[i];
    }

    /* Non-copyable/non-movable due to raw pointers */
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;
    Trie(Trie&& other) noexcept : type(other.type), close(std::move(other.close)) {
        for (int i = 0; i < 256; i++) { table[i] = other.table[i]; other.table[i] = nullptr; }
    }
    Trie& operator=(Trie&& other) noexcept {
        if (this != &other) {
            for (int i = 0; i < 256; i++) delete table[i];
            type = other.type;
            close = std::move(other.close);
            for (int i = 0; i < 256; i++) { table[i] = other.table[i]; other.table[i] = nullptr; }
        }
        return *this;
    }

    void insert(int tokenType, const std::vector<uint8_t>& token);
    void insertClose(int tokenType, const std::vector<uint8_t>& openToken, const std::vector<uint8_t>& closeToken);
    /* Returns (type, depth, closeBytes). If type==0, no match. */
    struct MatchResult { int type; int depth; std::vector<uint8_t> close; };
    MatchResult match(const uint8_t* token, size_t len) const;
};

#include <memory>

/* LanguageFeature pre-processed for fast matching */
struct LanguageFeature {
    std::unique_ptr<Trie> complexity;
    std::unique_ptr<Trie> multiLineComments;
    std::vector<std::vector<std::string>> multiLine;
    std::unique_ptr<Trie> singleLineComments;
    std::vector<std::string> lineComment;
    std::unique_ptr<Trie> strings;
    std::unique_ptr<Trie> tokens;
    bool nested = false;
    std::vector<std::vector<uint8_t>> postfixExcludes;
    uint8_t complexityCheckMask = 0;
    uint8_t singleLineCommentMask = 0;
    uint8_t multiLineCommentMask = 0;
    uint8_t stringCheckMask = 0;
    uint8_t processMask = 0;
    std::vector<std::string> keywords;
    std::vector<std::vector<uint8_t>> keywordBytes;
    std::vector<Quote> quotes;

    /* unique_ptr handles cleanup + move automatically */
};

/* File processing job */
struct FileJob {
    std::string language;
    std::vector<std::string> possibleLanguages;
    std::string filename;
    std::string extension;
    std::string location;
    std::string symlocation;
    std::vector<uint8_t> content;
    int64_t bytes = 0;
    int64_t lines = 0;
    int64_t code = 0;
    int64_t comment = 0;
    int64_t blank = 0;
    int64_t complexity = 0;
    std::vector<int64_t> complexityLine;
    double weightedComplexity = 0.0;
    bool binary = false;
    bool minified = false;
    bool generated = false;
    int uloc = 0;
    std::vector<int> lineLength;
    bool classifyContent = false;
    std::vector<uint8_t> contentByteType;
    bool trackComplexityLines = false;
};

/* Language summary for aggregation */
struct LanguageSummary {
    std::string name;
    int64_t bytes = 0;
    int64_t lines = 0;
    int64_t code = 0;
    int64_t comment = 0;
    int64_t blank = 0;
    int64_t complexity = 0;
    int64_t count = 0;
    double weightedComplexity = 0.0;
    std::vector<FileJob*> files;
    std::vector<int> lineLength;
    int uloc = 0;
};

/* Duplicate hash checker */
struct CheckDuplicates {
    std::unordered_map<int64_t, std::vector<std::vector<uint8_t>>> hashes;
    std::mutex mux;

    void add(int64_t key, const std::vector<uint8_t>& hash);
    bool check(int64_t key, const std::vector<uint8_t>& hash);
};

/* SheBang constant */
const std::string SheBang = "#!";

#endif
