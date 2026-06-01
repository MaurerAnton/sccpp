#include "language.hpp"
#include "languages_json.h"
#include <nlohmann/json.hpp>
#include <string>
#include <cstring>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

std::unordered_map<std::string, Language> languageDatabase;
std::unordered_map<std::string, std::vector<std::string>> extensionToLanguage;
std::unordered_map<std::string, std::string> filenameToLanguage;
std::unordered_map<std::string, std::vector<std::string>> shebangLookup;
std::unordered_map<std::string, LanguageFeature> languageFeatures;
std::mutex languageFeaturesMutex;

static std::string toLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::vector<uint8_t> toBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

void initLanguageDatabase() {
    std::string jsonStr(reinterpret_cast<const char*>(languages_json), languages_json_len);
    auto db = json::parse(jsonStr);

    for (auto& [name, val] : db.items()) {
        Language lang;

        if (val.contains("line_comment")) {
            for (auto& s : val["line_comment"]) lang.lineComment.push_back(s.get<std::string>());
        }
        if (val.contains("complexitychecks")) {
            for (auto& s : val["complexitychecks"]) lang.complexityChecks.push_back(s.get<std::string>());
        }
        if (val.contains("complexitychecks_postfix")) {
            for (auto& s : val["complexitychecks_postfix"]) lang.complexityChecksPostfix.push_back(s.get<std::string>());
        }
        if (val.contains("complexitychecks_postfix_excludes")) {
            for (auto& s : val["complexitychecks_postfix_excludes"]) lang.complexityChecksPostfixExcludes.push_back(s.get<std::string>());
        }
        if (val.contains("extensions")) {
            for (auto& s : val["extensions"]) lang.extensions.push_back(s.get<std::string>());
        }
        if (val.contains("multi_line")) {
            for (auto& ml : val["multi_line"]) {
                std::vector<std::string> pair;
                pair.push_back(ml[0].get<std::string>());
                pair.push_back(ml[1].get<std::string>());
                lang.multiLine.push_back(pair);
            }
        }
        if (val.contains("quotes")) {
            for (auto& q : val["quotes"]) {
                Quote quote;
                quote.start = q["start"].get<std::string>();
                quote.end = q["end"].get<std::string>();
                if (q.contains("ignoreEscape")) quote.ignoreEscape = q["ignoreEscape"].get<bool>();
                if (q.contains("docString")) quote.docString = q["docString"].get<bool>();
                lang.quotes.push_back(quote);
            }
        }
        if (val.contains("keywords")) {
            for (auto& s : val["keywords"]) lang.keywords.push_back(s.get<std::string>());
        }
        if (val.contains("filenames")) {
            for (auto& s : val["filenames"]) lang.fileNames.push_back(s.get<std::string>());
        }
        if (val.contains("shebangs")) {
            for (auto& s : val["shebangs"]) lang.sheBangs.push_back(s.get<std::string>());
        }
        if (val.contains("extensionFile")) lang.extensionFile = val["extensionFile"].get<bool>();
        if (val.contains("nestedmultiline")) lang.nestedMultiLine = val["nestedmultiline"].get<bool>();

        languageDatabase[name] = lang;

        /* Build extension maps */
        for (const auto& ext : lang.extensions) {
            extensionToLanguage[ext].push_back(name);
        }
        for (const auto& fn : lang.fileNames) {
            filenameToLanguage[fn] = name;
        }
        if (!lang.sheBangs.empty()) {
            shebangLookup[name] = lang.sheBangs;
        }
    }
}

void processLanguageFeature(const std::string& name, const Language& lang, LanguageFeature& feat) {
    (void)name;
    std::unique_ptr<Trie> complexityTrie = std::make_unique<Trie>();
    std::unique_ptr<Trie> slCommentTrie = std::make_unique<Trie>();
    std::unique_ptr<Trie> mlCommentTrie = std::make_unique<Trie>();
    std::unique_ptr<Trie> stringTrie = std::make_unique<Trie>();
    std::unique_ptr<Trie> tokenTrie = std::make_unique<Trie>();

    uint8_t complexityMask = 0;
    uint8_t singleLineCommentMask = 0;
    uint8_t multiLineCommentMask = 0;
    uint8_t stringMask = 0;
    uint8_t processMask = 0;

    for (const auto& v : lang.complexityChecks) {
        if (!v.empty()) {
            complexityMask |= (uint8_t)v[0];
            complexityTrie->insert(T_COMPLEXITY, toBytes(v));
            tokenTrie->insert(T_COMPLEXITY, toBytes(v));
        }
    }

    for (const auto& v : lang.complexityChecksPostfix) {
        if (!v.empty()) {
            tokenTrie->insert(T_COMPLEXITY_POSTFIX, toBytes(v));
            processMask |= (uint8_t)v[0];
        }
    }

    for (const auto& v : lang.complexityChecksPostfixExcludes) {
        feat.postfixExcludes.push_back(toBytes(v));
    }

    for (const auto& v : lang.lineComment) {
        if (!v.empty()) {
            singleLineCommentMask |= (uint8_t)v[0];
            slCommentTrie->insert(T_SLCOMMENT, toBytes(v));
            tokenTrie->insert(T_SLCOMMENT, toBytes(v));
        }
    }
    processMask |= singleLineCommentMask;

    for (const auto& v : lang.multiLine) {
        if (v.size() >= 2 && !v[0].empty()) {
            multiLineCommentMask |= (uint8_t)v[0][0];
            mlCommentTrie->insertClose(T_MLCOMMENT, toBytes(v[0]), toBytes(v[1]));
            tokenTrie->insertClose(T_MLCOMMENT, toBytes(v[0]), toBytes(v[1]));
        }
    }
    processMask |= multiLineCommentMask;

    for (const auto& v : lang.quotes) {
        if (!v.start.empty()) {
            stringMask |= (uint8_t)v.start[0];
            stringTrie->insertClose(T_STRING, toBytes(v.start), toBytes(v.end));
            tokenTrie->insertClose(T_STRING, toBytes(v.start), toBytes(v.end));
        }
    }
    processMask |= stringMask;

    for (const auto& v : lang.keywords) {
        feat.keywordBytes.push_back(toBytes(v));
    }

    feat.complexity = std::move(complexityTrie);
    feat.multiLineComments = std::move(mlCommentTrie);
    feat.multiLine = lang.multiLine;
    feat.singleLineComments = std::move(slCommentTrie);
    feat.lineComment = lang.lineComment;
    feat.strings = std::move(stringTrie);
    feat.tokens = std::move(tokenTrie);
    feat.nested = lang.nestedMultiLine;
    feat.complexityCheckMask = complexityMask;
    feat.multiLineCommentMask = multiLineCommentMask;
    feat.singleLineCommentMask = singleLineCommentMask;
    feat.stringCheckMask = stringMask;
    feat.processMask = processMask;
    feat.keywords = lang.keywords;
    feat.quotes = lang.quotes;
}

void loadLanguageFeature(const std::string& name) {
    std::lock_guard<std::mutex> lock(languageFeaturesMutex);
    if (languageFeatures.find(name) != languageFeatures.end()) return;

    auto it = languageDatabase.find(name);
    if (it == languageDatabase.end()) return;

    LanguageFeature feat;
    processLanguageFeature(name, it->second, feat);
    languageFeatures[name] = std::move(feat);
}

void setupCountAs(const std::string& countAs) {
    if (countAs.empty()) return;

    size_t pos = 0;
    std::string s = countAs;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        std::string pair = (comma == std::string::npos) ? s.substr(pos) : s.substr(pos, comma - pos);
        pos = (comma == std::string::npos) ? s.size() : comma + 1;

        size_t colon = pair.find(':');
        if (colon == std::string::npos) continue;

        std::string ext = toLower(pair.substr(0, colon));
        std::string target = pair.substr(colon + 1);

        /* Try to match by language name */
        bool identified = false;
        for (auto& [name, lang] : languageDatabase) {
            if (toLower(name) == toLower(target)) {
                extensionToLanguage[ext] = {name};
                identified = true;
                break;
            }
        }

        /* If not found, try by extension */
        if (!identified) {
            auto it = extensionToLanguage.find(toLower(target));
            if (it != extensionToLanguage.end()) {
                extensionToLanguage[ext] = it->second;
            }
        }
    }
}
