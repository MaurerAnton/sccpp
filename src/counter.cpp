#include "counter.hpp"
#include "language.hpp"
#include <cstring>
#include <algorithm>

/* ---- Helper functions ---- */

static inline bool isWhitespace(uint8_t b) {
    return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}

static inline bool isIdentContinue(uint8_t b) {
    return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') || b == '_';
}

static bool checkForMatchSingle(const uint8_t* content, int index, int endPoint, const std::vector<uint8_t>& matches) {
    if (matches.empty() || content[index] != matches[0]) return false;
    for (size_t j = 0; j < matches.size(); j++) {
        if (index + (int)j >= endPoint + 1 || matches[j] != content[index + j]) return false;
    }
    return true;
}

static int nextNonWhitespaceIdx(const std::vector<uint8_t>& c, int idx) {
    while (idx < (int)c.size() && isWhitespace(c[idx])) idx++;
    return idx;
}

static bool hasPostfixExclude(const std::vector<uint8_t>& content, int index, int offsetJump,
                               const std::vector<std::vector<uint8_t>>& excludes) {
    if (index + offsetJump > (int)content.size()) return false;
    for (auto& exclude : excludes) {
        if ((int)exclude.size() < offsetJump) continue;
        bool match = true;
        for (int k = 0; k < offsetJump; k++) {
            if (content[index + k] != exclude[k]) { match = false; break; }
        }
        if (!match) continue;
        if ((int)exclude.size() == offsetJump) return true;
        int next = nextNonWhitespaceIdx(content, index + offsetJump);
        int remaining = (int)exclude.size() - offsetJump;
        if (next + remaining > (int)content.size()) continue;
        match = true;
        for (int k = 0; k < remaining; k++) {
            if (content[next + k] != exclude[offsetJump + k]) { match = false; break; }
        }
        if (!match) continue;
        int after = next + remaining;
        if (isIdentContinue(exclude.back()))
            return after == (int)content.size() || !isIdentContinue(content[after]);
        return true;
    }
    return false;
}

static bool hasNonWhitespaceBefore(const std::vector<uint8_t>& content, int idx) {
    for (int i = idx - 1; i >= 0; i--)
        if (!isWhitespace(content[i])) return true;
    return false;
}

static void countComplexityPostfix(FileJob* job, int index, int offsetJump,
                                    const std::vector<std::vector<uint8_t>>& excludes) {
    if (index == 0) return;
    auto& c = job->content;
    if (isWhitespace(c[index - 1]) && !hasNonWhitespaceBefore(c, index - 1)) return;
    if (!excludes.empty() && hasPostfixExclude(c, index, offsetJump, excludes)) return;
    job->complexity++;
    if (!job->complexityLine.empty()) job->complexityLine.back()++;
}

static void bumpComplexityLine(FileJob* job) {
    if (!job->complexityLine.empty()) job->complexityLine.back()++;
}

/* ByteType for classifyContent */
static uint8_t stateToByteType(int64_t st) {
    switch (st) {
        case S_CODE: return BYTE_CODE;
        case S_STRING: return BYTE_STRING;
        case S_COMMENT: case S_COMMENT_CODE: case S_MULTICOMMENT:
        case S_MULTICOMMENT_CODE: case S_MULTICOMMENT_BLANK: case S_DOCSTRING:
            return BYTE_COMMENT;
        default: return BYTE_BLANK;
    }
}

static int64_t resetState(int64_t st) {
    if (st == S_MULTICOMMENT || st == S_MULTICOMMENT_CODE) return S_MULTICOMMENT;
    if (st == S_STRING) return S_STRING;
    return S_BLANK;
}

/* ---- State machine functions (return new index and new state) ---- */

struct StateResult {
    int index;
    int64_t state;
};

static StateResult codeState(FileJob* job, int index, int endPoint, int64_t currentState,
                              std::vector<uint8_t>& endString, std::vector<std::vector<uint8_t>>& endComments,
                              const LanguageFeature& feat, bool& ignoreEscape, bool skipComplexity,
                              bool checkBinary) {
    auto& content = job->content;
    for (int i = index; i <= endPoint; i++) {
        uint8_t cur = content[i];

        if (job->classifyContent && (size_t)i < job->contentByteType.size())
            job->contentByteType[i] = BYTE_CODE;

        if (cur == '\n') return {i, currentState};

        if (checkBinary && i < 10000 && cur == 0) { job->binary = true; return {i, currentState}; }

        if (cur & feat.processMask) {
            auto m = feat.tokens->match(&content[i], content.size() - i);
            if (m.type != 0) {
                switch (m.type) {
                    case T_STRING: {
                        bool ie = false;
                        for (auto& q : feat.quotes) {
                            if ((q.ignoreEscape || q.docString) && !q.start.empty()) {
                                bool match = true;
                                for (size_t j = 0; j < q.start.size() && i + j < content.size(); j++)
                                    if (content[i + j] != (uint8_t)q.start[j]) { match = false; break; }
                                if (match) { ie = q.ignoreEscape; break; }
                            }
                        }
                        if (i > 0 && content[i - 1] != '\\') currentState = S_STRING;
                        endString = m.close;
                        ignoreEscape = ie;
                        return {i, currentState};
                    }
                    case T_SLCOMMENT:
                        return {i, S_COMMENT_CODE};
                    case T_MLCOMMENT:
                        if (feat.nested || endComments.empty()) {
                            endComments.push_back(m.close);
                            return {i + m.depth - 1, S_MULTICOMMENT_CODE};
                        }
                        break;
                    case T_COMPLEXITY:
                        if (!skipComplexity) {
                            if (i == 0 || isWhitespace(content[i - 1])) {
                                job->complexity++;
                                bumpComplexityLine(job);
                            }
                        }
                        break;
                    case T_COMPLEXITY_POSTFIX:
                        if (!skipComplexity)
                            countComplexityPostfix(job, i, m.depth + 1, feat.postfixExcludes);
                        break;
                }
            }
        }
    }
    return {index, currentState};
}

static StateResult stringState(FileJob* job, int index, int endPoint,
                                const std::vector<uint8_t>& endString, int64_t currentState, bool ignoreEscape) {
    auto& content = job->content;
    for (int i = index; i <= endPoint; i++) {
        if (job->classifyContent && (size_t)i < job->contentByteType.size())
            job->contentByteType[i] = BYTE_STRING;

        if (content[i] == '\n') return {i, currentState};

        bool escaped = false;
        if (i > 0 && content[i - 1] == '\\') {
            int numEsc = 0;
            for (int j = i - 1; j > 0 && content[j] == '\\'; j--) numEsc++;
            if (numEsc % 2 != 0) escaped = true;
        }

        if (ignoreEscape || !escaped) {
            if (!endString.empty() && checkForMatchSingle(content.data(), i, endPoint, endString))
                return {i, S_CODE};
        }
    }
    return {index, currentState};
}

static StateResult docStringState(FileJob* job, int index, int endPoint,
                                   const std::vector<uint8_t>& endString, int64_t currentState) {
    auto& content = job->content;
    for (int i = index; i <= endPoint; i++) {
        if (job->classifyContent && (size_t)i < job->contentByteType.size())
            job->contentByteType[i] = BYTE_COMMENT;

        if (content[i] == '\n') return {i, currentState};

        if (i > 0 && content[i - 1] != '\\') {
            if (!endString.empty() && checkForMatchSingle(content.data(), i, endPoint, endString)) {
                /* Check if only whitespace follows until newline */
                for (int j = i + (int)endString.size(); j <= endPoint; j++) {
                    if (content[j] == '\n') return {i, S_COMMENT};
                    if (!isWhitespace(content[j])) return {i, S_CODE};
                }
                return {i, S_CODE};
            }
        }
    }
    return {index, currentState};
}

static StateResult commentState(FileJob* job, int index, int endPoint, int64_t currentState,
                                 std::vector<std::vector<uint8_t>>& endComments,
                                 const LanguageFeature& feat) {
    auto& content = job->content;
    for (int i = index; i <= endPoint; i++) {
        if (job->classifyContent && (size_t)i < job->contentByteType.size())
            job->contentByteType[i] = BYTE_COMMENT;

        if (content[i] == '\n') return {i, currentState};

        if (!endComments.empty()) {
            if (checkForMatchSingle(content.data(), i, endPoint, endComments.back())) {
                int oj = (int)endComments.back().size();
                endComments.pop_back();
                if (endComments.empty()) {
                    currentState = (currentState == S_MULTICOMMENT_CODE) ? S_CODE : S_MULTICOMMENT_BLANK;
                }
                return {i + oj - 1, currentState};
            }
        }

        if (feat.nested || endComments.empty()) {
            auto m = feat.multiLineComments->match(&content[i], content.size() - i);
            if (m.type == T_MLCOMMENT) {
                endComments.push_back(m.close);
                return {i + m.depth - 1, currentState};
            }
        }
    }
    return {index, currentState};
}

static StateResult blankState(FileJob* job, int index, int64_t currentState,
                               std::vector<std::vector<uint8_t>>& endComments,
                               std::vector<uint8_t>& endString,
                               const LanguageFeature& feat, bool& ignoreEscape,
                               bool skipComplexity) {
    auto& content = job->content;
    auto m = feat.tokens->match(&content[index], content.size() - index);

    switch (m.type) {
        case T_MLCOMMENT:
            if (feat.nested || endComments.empty()) {
                endComments.push_back(m.close);
                return {index + m.depth - 1, S_MULTICOMMENT};
            }
            break;
        case T_SLCOMMENT:
            return {index, S_COMMENT};
        case T_STRING: {
            bool ie = false;
            for (auto& q : feat.quotes) {
                if ((q.ignoreEscape || q.docString) && !q.start.empty()) {
                    bool match = true;
                    for (size_t j = 0; j < q.start.size() && index + j < content.size(); j++)
                        if (content[index + j] != (uint8_t)q.start[j]) { match = false; break; }
                    if (match) { ie = q.ignoreEscape; break; }
                }
            }
            ignoreEscape = ie;
            endString = m.close;
            bool isDS = false;
            for (auto& q : feat.quotes)
                if (q.docString && q.end == std::string(endString.begin(), endString.end())) isDS = true;
            return {index, isDS ? S_DOCSTRING : S_STRING};
        }
        case T_COMPLEXITY:
            if (!skipComplexity) {
                if (index == 0 || isWhitespace(content[index - 1])) {
                    job->complexity++;
                    bumpComplexityLine(job);
                }
            }
            return {index, S_CODE};
        case T_COMPLEXITY_POSTFIX:
            if (!skipComplexity)
                countComplexityPostfix(job, index, m.depth + 1, feat.postfixExcludes);
            return {index, S_CODE};
        default:
            return {index, S_CODE};
    }
    return {index, currentState};
}

/* ---- Main CountStats ---- */

void countStats(FileJob* job, const CountOptions& opts) {
    if (job->bytes == 0) { job->lines = 0; return; }

    auto it = languageFeatures.find(job->language);
    if (it == languageFeatures.end()) return;
    const LanguageFeature& feat = it->second;

    int endPoint = (int)job->bytes - 1;
    int64_t currentState = S_BLANK;
    std::vector<std::vector<uint8_t>> endComments;
    std::vector<uint8_t> endString;
    bool ignoreEscape = false;

    if (job->trackComplexityLines) job->complexityLine.push_back(0);
    if (job->classifyContent) job->contentByteType.assign(job->bytes, 0);

    auto& content = job->content;

    /* BOM skip: UTF-8 BOM = EF BB BF */
    int startIdx = 0;
    if (content.size() >= 3 && content[0] == 0xEF && content[1] == 0xBB && content[2] == 0xBF)
        startIdx = 3;

    for (int index = startIdx; index < (int)content.size(); index++) {
        if (job->classifyContent && (size_t)index < job->contentByteType.size())
            job->contentByteType[index] = stateToByteType(currentState);

        if (!isWhitespace(content[index])) {
            StateResult sr = {index, currentState};
            switch (currentState) {
                case S_CODE:
                    sr = codeState(job, index, endPoint, currentState, endString, endComments, feat, ignoreEscape, opts.skipComplexity, opts.checkBinary);
                    break;
                case S_STRING:
                    sr = stringState(job, index, endPoint, endString, currentState, ignoreEscape);
                    break;
                case S_DOCSTRING:
                    sr = docStringState(job, index, endPoint, endString, currentState);
                    break;
                case S_MULTICOMMENT:
                case S_MULTICOMMENT_CODE:
                    sr = commentState(job, index, endPoint, currentState, endComments, feat);
                    break;
                case S_BLANK:
                case S_MULTICOMMENT_BLANK:
                    sr = blankState(job, index, currentState, endComments, endString, feat, ignoreEscape, opts.skipComplexity);
                    break;
            }
            index = sr.index;
            currentState = sr.state;
        }

        if (index >= (int)content.size()) return;
        if (index < 10000 && job->binary) return;

        if (content[index] == '\n' || index >= endPoint) {
            job->lines++;
            if (job->trackComplexityLines) job->complexityLine.push_back(0);

            switch (currentState) {
                case S_CODE: case S_STRING: case S_COMMENT_CODE: case S_MULTICOMMENT_CODE:
                    job->code++;
                    break;
                case S_COMMENT: case S_MULTICOMMENT: case S_MULTICOMMENT_BLANK: case S_DOCSTRING:
                    job->comment++;
                    break;
                case S_BLANK:
                    job->blank++;
                    break;
                default:
                    job->blank++;
                    break;
            }
            currentState = resetState(currentState);
        }
    }

    /* ---- Post-processing: generated, minified, ULOC ---- */

    /* Generated file detection (check first 1000 bytes for markers) */
    if (opts.checkGenerated && !opts.generatedMarkers.empty()) {
        size_t headLen = std::min(job->content.size(), (size_t)1000);
        std::string head(job->content.begin(), job->content.begin() + headLen);
        /* Lowercase */
        for (auto& c : head) c = (char)std::tolower((unsigned char)c);
        for (const auto& marker : opts.generatedMarkers) {
            std::string mlow = marker;
            for (auto& c : mlow) c = (char)std::tolower((unsigned char)c);
            if (head.find(mlow) != std::string::npos) {
                job->generated = true;
                job->language += " (gen)";
                break;
            }
        }
    }

    /* Minified file detection (average line length) */
    if (!job->generated && opts.checkMinified && job->lines > 0) {
        int avgLineBytes = (int)job->content.size() / (int)job->lines;
        if (avgLineBytes >= opts.minGenLineLength) {
            job->minified = true;
            job->language += " (min)";
        }
    }

    /* Per-file ULOC */
    if (job->content.size() > 1) {
        /* Count unique lines (trim trailing newline, split by newline) */
        std::unordered_set<std::string> unique;
        size_t start = 0;
        auto& c = job->content;
        /* Skip trailing newline for splitting */
        size_t end = c.size();
        while (end > 0 && c[end - 1] == '\n') end--;
        for (size_t i = 0; i <= end; i++) {
            if (i == end || c[i] == '\n') {
                unique.insert(std::string(c.begin() + start, c.begin() + i));
                start = i + 1;
            }
        }
        job->uloc = (int)unique.size();
    }
}

/* ---- Global ULOC state ---- */

std::mutex ulocMutex;
std::unordered_set<std::string> ulocGlobalCount;
std::unordered_map<std::string, std::unordered_set<std::string>> ulocLanguageCount;

void trackUloc(FileJob* job) {
    if (job->content.empty()) return;
    std::lock_guard<std::mutex> lock(ulocMutex);
    size_t start = 0;
    auto& c = job->content;
    size_t end = c.size();
    while (end > 0 && c[end - 1] == '\n') end--;
    for (size_t i = 0; i <= end; i++) {
        if (i == end || c[i] == '\n') {
            std::string line(c.begin() + start, c.begin() + i);
            ulocGlobalCount.insert(line);
            ulocLanguageCount[job->language].insert(line);
            start = i + 1;
        }
    }
}

/* Simple FNV-1a 64-bit hash for duplicate detection */
uint64_t hashContent(const std::vector<uint8_t>& data) {
    uint64_t hash = 14695981039346656037ULL;
    for (uint8_t b : data) {
        hash ^= b;
        hash *= 1099511628211ULL;
    }
    return hash;
}
