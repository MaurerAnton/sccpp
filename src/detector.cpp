#include "detector.hpp"
#include "language.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cctype>

/* Extension cache */
static std::unordered_map<std::string, std::string> extensionCache;

std::string getExtension(const std::string& name) {
    std::string lower = name;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);

    auto it = extensionCache.find(lower);
    if (it != extensionCache.end()) return it->second;

    /* Find the last dot */
    size_t dot = name.rfind('.');
    std::string ext;

    if (dot == std::string::npos || dot == 0) {
        ext = lower;
    } else {
        /* Get extension after last dot, then get sub-extension */
        std::string baseExt = name.substr(dot); /* e.g. ".ts" */
        std::string base = name.substr(0, dot); /* e.g. "file.d" */
        size_t subDot = base.rfind('.');
        std::string subExt;
        if (subDot != std::string::npos && subDot != 0) {
            subExt = base.substr(subDot); /* e.g. ".d" */
        }
        ext = subExt + baseExt;
        /* Remove leading dot(s) */
        while (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
        if (ext.empty()) ext = lower;
    }

    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    extensionCache[lower] = ext;
    return ext;
}

std::pair<std::vector<std::string>, std::string> detectLanguage(const std::string& name) {
    std::string lowerName = name;
    for (auto& c : lowerName) c = (char)std::tolower((unsigned char)c);

    /* Check full filename match first */
    auto fnIt = filenameToLanguage.find(lowerName);
    if (fnIt != filenameToLanguage.end()) {
        return {{fnIt->second}, name};
    }

    /* Check for shebang candidates (no extension or dotfile) */
    int dotCount = 0;
    for (char c : name) if (c == '.') dotCount++;
    if (dotCount == 0 || (name.size() > 0 && name[0] == '.' && dotCount == 1)) {
        /* Could be a shebang file */
        return {{SheBang}, name};
    }

    /* Check full name in extension map */
    auto extIt = extensionToLanguage.find(lowerName);
    if (extIt != extensionToLanguage.end()) {
        return {extIt->second, lowerName};
    }

    /* Check by extension */
    std::string ext = getExtension(name);
    extIt = extensionToLanguage.find(ext);
    if (extIt != extensionToLanguage.end()) {
        return {extIt->second, ext};
    }

    /* Try again with stripped extension (e.g., d.ts -> ts) */
    std::string subExt = getExtension(ext);
    if (subExt != ext) {
        extIt = extensionToLanguage.find(subExt);
        if (extIt != extensionToLanguage.end()) {
            return {extIt->second, subExt};
        }
    }

    return {{}, ""};
}

static bool isWhitespaceB(uint8_t b) {
    return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}

std::string detectSheBang(const std::string& content) {
    if (content.size() < 2 || content[0] != '#' || content[1] != '!') {
        return "";
    }

    /* Extract first line */
    size_t nl = content.find('\n');
    std::string line = (nl == std::string::npos) ? content : content.substr(0, nl);

    /* Scan for the command */
    int state = 0;
    size_t lastSlash = 0;
    std::string candidate1, candidate2;

    for (size_t i = 0; i < line.size(); i++) {
        switch (state) {
            case 0: /* skip whitespace after #! */
                if (line[i] == '/') { lastSlash = i; state = 1; }
                break;
            case 1: /* after first / */
                if (line[i] == '/') lastSlash = i;
                if (i == line.size() - 1) {
                    candidate1 = line.substr(lastSlash + 1);
                }
                if (isWhitespaceB((uint8_t)line[i])) {
                    candidate1 = line.substr(lastSlash + 1, i - lastSlash - 1);
                    state = 2;
                }
                break;
            case 2: /* after candidate1 */
                if (!isWhitespaceB((uint8_t)line[i])) { lastSlash = i; state = 3; }
                break;
            case 3:
                if (i == line.size() - 1) {
                    candidate2 = line.substr(lastSlash);
                }
                if (isWhitespaceB((uint8_t)line[i])) {
                    candidate2 = line.substr(lastSlash, i - lastSlash);
                    state = 4;
                }
                break;
            case 4:
                break;
        }
    }

    std::string cmd;
    if (candidate1 == "env") {
        cmd = candidate2;
    } else if (!candidate1.empty()) {
        cmd = candidate1;
    }

    if (cmd.empty()) return "";

    /* Look up in shebang map */
    for (auto& [lang, shebangs] : shebangLookup) {
        for (auto& sb : shebangs) {
            if (sb == cmd) return lang;
        }
    }

    return "";
}

std::string determineLanguage(const std::string& filename,
                               const std::string& fallbackLanguage,
                               const std::vector<std::string>& possibleLanguages,
                               const std::vector<uint8_t>& content) {
    (void)filename;
    if (possibleLanguages.empty()) return fallbackLanguage;
    if (possibleLanguages.size() == 1) return possibleLanguages[0];

    /* Check first 20000 bytes */
    size_t checkLen = std::min(content.size(), (size_t)20000);

    struct LangGuess { std::string name; int count; };
    std::vector<LangGuess> guesses;
    std::string primary;

    for (const auto& lang : possibleLanguages) {
        std::lock_guard<std::mutex> lock(languageFeaturesMutex);
        auto it = languageFeatures.find(lang);
        if (it == languageFeatures.end()) {
            guesses.push_back({lang, 0});
            continue;
        }
        const auto& feat = it->second;

        int count = 0;
        for (const auto& key : feat.keywordBytes) {
            /* Search in first checkLen bytes */
            for (size_t i = 0; i + key.size() <= checkLen; i++) {
                bool match = true;
                for (size_t j = 0; j < key.size(); j++) {
                    if (content[i + j] != key[j]) { match = false; break; }
                }
                if (match) { count++; break; }
            }
        }

        if (feat.keywords.empty()) {
            primary = lang;
        }
        guesses.push_back({lang, count});
    }

    /* Sort by count descending, then name ascending */
    std::sort(guesses.begin(), guesses.end(), [](const LangGuess& a, const LangGuess& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.name < b.name;
    });

    if (!primary.empty() && !guesses.empty()) {
        if (guesses[0].count < 3) return primary;
    }

    if (!guesses.empty()) return guesses[0].name;
    return fallbackLanguage;
}

bool hardRemapLanguage(FileJob* job,
                       const std::vector<std::pair<std::string, std::string>>& remapAll,
                       const std::vector<std::pair<std::string, std::string>>& remapUnknown) {
    if (job->content.empty()) return false;

    size_t cutoff = std::min(job->content.size(), (size_t)1000);

    /* Check remapAll first */
    for (const auto& rule : remapAll) {
        std::vector<uint8_t> pattern(rule.first.begin(), rule.first.end());
        for (size_t i = 0; i + pattern.size() <= cutoff; i++) {
            bool match = true;
            for (size_t j = 0; j < pattern.size(); j++) {
                if (job->content[i + j] != pattern[j]) { match = false; break; }
            }
            if (match) {
                job->language = rule.second;
                return true;
            }
        }
    }

    /* Check remapUnknown */
    for (const auto& rule : remapUnknown) {
        std::vector<uint8_t> pattern(rule.first.begin(), rule.first.end());
        for (size_t i = 0; i + pattern.size() <= cutoff; i++) {
            bool match = true;
            for (size_t j = 0; j < pattern.size(); j++) {
                if (job->content[i + j] != pattern[j]) { match = false; break; }
            }
            if (match) {
                job->language = rule.second;
                return true;
            }
        }
    }

    return false;
}
