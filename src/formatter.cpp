#include "formatter.hpp"
#include "language.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <functional>

static std::string shortBreak(bool ci) {
    return ci
        ? "-------------------------------------------------------------------------------\n"
        : "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n";
}

static bool icaseEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

static std::function<bool(const LanguageSummary&, const LanguageSummary&)> makeLangSorter(const std::string& sortBy) {
    if (icaseEq(sortBy, "name") || icaseEq(sortBy, "names") ||
        icaseEq(sortBy, "language") || icaseEq(sortBy, "languages") ||
        icaseEq(sortBy, "lang") || icaseEq(sortBy, "langs")) {
        return [](const LanguageSummary& a, const LanguageSummary& b) {
            return a.name < b.name;
        };
    }
    if (icaseEq(sortBy, "line") || icaseEq(sortBy, "lines")) {
        return [](const LanguageSummary& a, const LanguageSummary& b) {
            if (a.lines != b.lines) return a.lines > b.lines;
            return a.name < b.name;
        };
    }
    if (icaseEq(sortBy, "blank") || icaseEq(sortBy, "blanks")) {
        return [](const LanguageSummary& a, const LanguageSummary& b) {
            if (a.blank != b.blank) return a.blank > b.blank;
            return a.name < b.name;
        };
    }
    if (icaseEq(sortBy, "code") || icaseEq(sortBy, "codes")) {
        return [](const LanguageSummary& a, const LanguageSummary& b) {
            if (a.code != b.code) return a.code > b.code;
            return a.name < b.name;
        };
    }
    if (icaseEq(sortBy, "comment") || icaseEq(sortBy, "comments")) {
        return [](const LanguageSummary& a, const LanguageSummary& b) {
            if (a.comment != b.comment) return a.comment > b.comment;
            return a.name < b.name;
        };
    }
    if (icaseEq(sortBy, "complexity") || icaseEq(sortBy, "complexitys") || icaseEq(sortBy, "comp")) {
        return [](const LanguageSummary& a, const LanguageSummary& b) {
            if (a.complexity != b.complexity) return a.complexity > b.complexity;
            return a.name < b.name;
        };
    }
    if (icaseEq(sortBy, "byte") || icaseEq(sortBy, "bytes")) {
        return [](const LanguageSummary& a, const LanguageSummary& b) {
            if (a.bytes != b.bytes) return a.bytes > b.bytes;
            return a.name < b.name;
        };
    }
    /* Default: files */
    return [](const LanguageSummary& a, const LanguageSummary& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.name < b.name;
    };
}

static std::function<bool(FileJob*, FileJob*)> makeFileSorter(const std::string& sortBy) {
    if (icaseEq(sortBy, "name") || icaseEq(sortBy, "names") ||
        icaseEq(sortBy, "language") || icaseEq(sortBy, "languages")) {
        return [](FileJob* a, FileJob* b) { return a->location < b->location; };
    }
    if (icaseEq(sortBy, "line") || icaseEq(sortBy, "lines")) {
        return [](FileJob* a, FileJob* b) { return a->lines > b->lines; };
    }
    if (icaseEq(sortBy, "blank") || icaseEq(sortBy, "blanks")) {
        return [](FileJob* a, FileJob* b) { return a->blank > b->blank; };
    }
    if (icaseEq(sortBy, "code") || icaseEq(sortBy, "codes")) {
        return [](FileJob* a, FileJob* b) { return a->code > b->code; };
    }
    if (icaseEq(sortBy, "comment") || icaseEq(sortBy, "comments")) {
        return [](FileJob* a, FileJob* b) { return a->comment > b->comment; };
    }
    if (icaseEq(sortBy, "complexity") || icaseEq(sortBy, "complexitys")) {
        return [](FileJob* a, FileJob* b) { return a->complexity > b->complexity; };
    }
    return [](FileJob* a, FileJob* b) { return a->lines > b->lines; };
}

std::string formatTabular(std::vector<FileJob*>& jobs, bool byFile,
                          bool noComplexity, bool /*more*/, bool ci,
                          bool /*noCocomo*/, bool /*noSize*/,
                          const std::string& sortBy) {

    std::string sb = shortBreak(ci);
    std::string out;

    /* Aggregate by language */
    std::unordered_map<std::string, LanguageSummary> langs;
    int64_t sumFiles = 0, sumLines = 0, sumCode = 0, sumComment = 0, sumBlank = 0, sumComplexity = 0;

    for (auto* job : jobs) {
        sumFiles++;
        sumLines += job->lines;
        sumCode += job->code;
        sumComment += job->comment;
        sumBlank += job->blank;
        sumComplexity += job->complexity;

        auto& ls = langs[job->language];
        if (ls.count == 0) {
            ls.name = job->language;
            ls.bytes = job->bytes;
            ls.lines = job->lines;
            ls.code = job->code;
            ls.comment = job->comment;
            ls.blank = job->blank;
            ls.complexity = job->complexity;
            ls.count = 1;
            if (byFile) ls.files.push_back(job);
        } else {
            ls.bytes += job->bytes;
            ls.lines += job->lines;
            ls.code += job->code;
            ls.comment += job->comment;
            ls.blank += job->blank;
            ls.complexity += job->complexity;
            ls.count++;
            if (byFile) ls.files.push_back(job);
        }
    }

    /* Sort languages */
    std::vector<LanguageSummary> sortedLangs;
    for (auto& [name, ls] : langs) sortedLangs.push_back(ls);
    auto langSorter = makeLangSorter(sortBy);
    std::sort(sortedLangs.begin(), sortedLangs.end(), langSorter);

    auto fileSorter = makeFileSorter(sortBy);

    /* Header */
    out += sb;
    char buf[256];
    if (!noComplexity) {
        snprintf(buf, sizeof(buf), "%-15s %9s %11s %9s %9s %10s %10s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code", "Complexity");
    } else {
        snprintf(buf, sizeof(buf), "%-21s %11s %11s %10s %11s %10s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code");
    }
    out += buf;

    if (!byFile) out += sb;

    /* Language rows */
    for (auto& ls : sortedLangs) {
        if (byFile) out += sb;

        std::string name = ls.name;
        if (!noComplexity && name.size() > 15) name = name.substr(0, 14) + "\xe2\x80\xa6";
        if (noComplexity && name.size() > 21) name = name.substr(0, 20) + "\xe2\x80\xa6";

        if (!noComplexity) {
            snprintf(buf, sizeof(buf), "%-15s %9ld %11ld %9ld %9ld %10ld %10ld\n",
                     name.c_str(),
                     ls.count, ls.lines, ls.blank, ls.comment, ls.code, ls.complexity);
        } else {
            snprintf(buf, sizeof(buf), "%-21s %11ld %11ld %10ld %11ld %10ld\n",
                     name.c_str(),
                     ls.count, ls.lines, ls.blank, ls.comment, ls.code);
        }
        out += buf;

        if (byFile) {
            std::sort(ls.files.begin(), ls.files.end(), fileSorter);
            out += sb;
            for (auto* f : ls.files) {
                std::string loc = f->location;
                if (!noComplexity) {
                    if (loc.size() > 26) loc = "~" + loc.substr(loc.size() - 25);
                    snprintf(buf, sizeof(buf), "%-27s %9ld %9ld %9ld %10ld %10ld\n",
                             loc.c_str(), f->lines, f->blank, f->comment, f->code, f->complexity);
                } else {
                    if (loc.size() > 33) loc = "~" + loc.substr(loc.size() - 32);
                    snprintf(buf, sizeof(buf), "%-34s %10ld %10ld %11ld %10ld\n",
                             loc.c_str(), f->lines, f->blank, f->comment, f->code);
                }
                out += buf;
            }
        }
    }

    /* Total row */
    out += sb;
    if (!noComplexity) {
        snprintf(buf, sizeof(buf), "%-15s %9ld %11ld %9ld %9ld %10ld %10ld\n",
                 "Total", sumFiles, sumLines, sumBlank, sumComment, sumCode, sumComplexity);
    } else {
        snprintf(buf, sizeof(buf), "%-21s %11ld %11ld %10ld %11ld %10ld\n",
                 "Total", sumFiles, sumLines, sumBlank, sumComment, sumCode);
    }
    out += buf;
    out += sb;

    return out;
}

std::string formatLanguages() {
    std::string out;
    std::vector<std::string> names;
    for (auto& [name, lang] : languageDatabase) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        std::string la = a, lb = b;
        for (auto& c : la) c = std::tolower(c);
        for (auto& c : lb) c = std::tolower(c);
        return la < lb;
    });

    for (auto& name : names) {
        out += name + " (";
        auto& lang = languageDatabase[name];
        for (size_t i = 0; i < lang.extensions.size(); i++) {
            if (i > 0) out += ",";
            out += lang.extensions[i];
        }
        for (size_t i = 0; i < lang.fileNames.size(); i++) {
            out += ",";
            out += lang.fileNames[i];
        }
        out += ")\n";
    }
    return out;
}
