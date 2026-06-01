#include "gitignore.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cctype>

void GitignoreMatcher::clear() {
    patterns_.clear();
}

int GitignoreMatcher::loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return 0;
    std::string line;
    int count = 0;
    while (std::getline(f, line)) {
        /* Strip trailing \r */
        if (!line.empty() && line.back() == '\r') line.pop_back();
        /* Strip leading/trailing whitespace */
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;
        size_t end = line.size();
        while (end > start && (line[end-1] == ' ' || line[end-1] == '\t')) end--;
        line = line.substr(start, end - start);

        /* Skip empty lines and comments */
        if (line.empty() || line[0] == '#') continue;

        addPattern(line);
        count++;
    }
    return count;
}

void GitignoreMatcher::addPattern(const std::string& line) {
    Pattern p;
    std::string pat = line;

    /* Negation */
    if (!pat.empty() && pat[0] == '!') {
        p.negated = true;
        pat = pat.substr(1);
    }

    /* Trailing / means directory only */
    if (!pat.empty() && pat.back() == '/') {
        p.dirOnly = true;
        pat = pat.substr(0, pat.size() - 1);
    }

    /* Leading / anchors to root */
    if (!pat.empty() && pat[0] == '/') {
        pat = pat.substr(1);
    }

    /* If pattern contains no / (except at start), it matches anywhere */
    p.matchBase = (pat.find('/') == std::string::npos);

    p.pattern = pat;
    patterns_.push_back(p);
}

bool GitignoreMatcher::isIgnored(const std::string& path, bool isDir) const {
    bool ignored = false;
    for (const auto& p : patterns_) {
        if (matchPattern(path, isDir, p)) {
            ignored = !p.negated;
        }
    }
    return ignored;
}

bool GitignoreMatcher::matchPattern(const std::string& path, bool isDir, const Pattern& p) const {
    /* Directory-only patterns only match directories */
    if (p.dirOnly && !isDir) return false;

    const std::string& pat = p.pattern;

    if (p.matchBase) {
        /* Pattern matches any path component */
        /* Check the full path */
        if (matchGlob(pat, path)) return true;
        /* Check just the filename */
        size_t slash = path.rfind('/');
        std::string filename = (slash != std::string::npos) ? path.substr(slash + 1) : path;
        if (matchGlob(pat, filename)) return true;
        /* Check any parent directory component */
        std::string tmp = path;
        while (true) {
            size_t s = tmp.rfind('/');
            if (s == std::string::npos) break;
            tmp = tmp.substr(0, s);
            if (matchGlob(pat, tmp)) return true;
        }
    } else {
        /* Anchored pattern — match from start */
        if (matchGlob(pat, path)) return true;
    }

    return false;
}

/* Simple glob matching: supports * and ? wildcards, ** for recursive matching */
bool GitignoreMatcher::matchGlob(const std::string& pattern, const std::string& str) {
    size_t pi = 0, si = 0;
    size_t pLen = pattern.size(), sLen = str.size();
    size_t starIdx = std::string::npos;
    size_t matchIdx = 0;

    while (si < sLen) {
        if (pi < pLen && pattern[pi] == '*') {
            /* Check for ** (matches across path separators) */
            if (pi + 1 < pLen && pattern[pi + 1] == '*') {
                /* ** matches everything including / */
                pi += 2;
                /* Skip optional / after ** */
                if (pi < pLen && pattern[pi] == '/') pi++;
                starIdx = pi;
                matchIdx = si;
                if (pi >= pLen) return true; /* ** at end matches rest */
            } else {
                /* Single * — matches within a single path component */
                starIdx = pi;
                matchIdx = si;
                pi++;
            }
        } else if (pi < pLen && (pattern[pi] == '?' || pattern[pi] == str[si])) {
            pi++; si++;
        } else if (starIdx != std::string::npos) {
            /* Backtrack to last * */
            if (starIdx < pLen && pattern[starIdx] == '*' && (starIdx + 1 >= pLen || pattern[starIdx + 1] != '*')) {
                /* Single * — don't cross path separators */
                if (str[matchIdx] == '/') return false;
            }
            pi = starIdx;
            matchIdx++;
            si = matchIdx;
        } else {
            return false;
        }
    }

    /* Consume remaining pattern chars (trailing stars match nothing) */
    while (pi < pLen && pattern[pi] == '*') pi++;
    return pi >= pLen;
}
