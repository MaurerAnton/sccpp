#include "formatter.hpp"
#include "language.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <sstream>

/* ---- Helpers ---- */

static std::string shortBreak(bool ci) {
    return ci
        ? "-------------------------------------------------------------------------------\n"
        : "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n";
}

static std::string wideBreak(bool ci) {
    return ci
        ? "-------------------------------------------------------------------------------------------------------------\n"
        : "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n";
}

static bool icaseEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

/* JSON string escaping */
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

/* ---- Aggregation ---- */

static void aggregate(const std::vector<FileJob*>& jobs,
                      std::vector<LanguageSummary>& sortedLangs,
                      int64_t& sumFiles, int64_t& sumLines, int64_t& sumCode,
                      int64_t& sumComment, int64_t& sumBlank, int64_t& sumComplexity,
                      bool byFile, const std::string& sortBy) {

    std::unordered_map<std::string, LanguageSummary> langs;
    sumFiles = sumLines = sumCode = sumComment = sumBlank = sumComplexity = 0;

    for (auto* job : jobs) {
        sumFiles++; sumLines += job->lines; sumCode += job->code;
        sumComment += job->comment; sumBlank += job->blank; sumComplexity += job->complexity;

        /* Compute per-file weighted complexity (matching Go's logic) */
        double wc = job->code ? (double)job->complexity / (double)job->code * 100.0 : 0.0;
        job->weightedComplexity = wc;

        auto& ls = langs[job->language];
        if (ls.count == 0) {
            ls.name = job->language; ls.bytes = job->bytes;
            ls.lines = job->lines; ls.code = job->code;
            ls.comment = job->comment; ls.blank = job->blank;
            ls.complexity = job->complexity;
            ls.weightedComplexity = wc;
            ls.count = 1;
            if (byFile) ls.files.push_back(job);
        } else {
            ls.bytes += job->bytes; ls.lines += job->lines; ls.code += job->code;
            ls.comment += job->comment; ls.blank += job->blank;
            ls.complexity += job->complexity;
            ls.weightedComplexity += wc;
            ls.count++;
            if (byFile) ls.files.push_back(job);
        }
    }

    for (auto& [name, ls] : langs) sortedLangs.push_back(ls);

    /* Sort (default: files desc, name asc) */
    auto cmp = [&](const LanguageSummary& a, const LanguageSummary& b) {
        if (icaseEq(sortBy, "name") || icaseEq(sortBy, "names")) return a.name < b.name;
        if (icaseEq(sortBy, "line") || icaseEq(sortBy, "lines")) { if (a.lines != b.lines) return a.lines > b.lines; return a.name < b.name; }
        if (icaseEq(sortBy, "blank") || icaseEq(sortBy, "blanks")) { if (a.blank != b.blank) return a.blank > b.blank; return a.name < b.name; }
        if (icaseEq(sortBy, "code") || icaseEq(sortBy, "codes")) { if (a.code != b.code) return a.code > b.code; return a.name < b.name; }
        if (icaseEq(sortBy, "comment") || icaseEq(sortBy, "comments")) { if (a.comment != b.comment) return a.comment > b.comment; return a.name < b.name; }
        if (icaseEq(sortBy, "complexity") || icaseEq(sortBy, "comp")) { if (a.complexity != b.complexity) return a.complexity > b.complexity; return a.name < b.name; }
        if (icaseEq(sortBy, "byte") || icaseEq(sortBy, "bytes")) { if (a.bytes != b.bytes) return a.bytes > b.bytes; return a.name < b.name; }
        if (a.count != b.count) return a.count > b.count;
        return a.name < b.name;
    };
    std::sort(sortedLangs.begin(), sortedLangs.end(), cmp);

    /* Sort per-file lists */
    if (byFile) {
        for (auto& ls : sortedLangs) {
            std::sort(ls.files.begin(), ls.files.end(), [](FileJob* a, FileJob* b) {
                return a->lines > b->lines;
            });
        }
    }
}

/* ---- Tabular ---- */

std::string formatTabular(std::vector<FileJob*>& jobs, bool byFile,
                          bool noComplexity, bool wide, bool ci,
                          const std::string& sortBy) {

    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, byFile, sortBy);

    std::string brk = wide ? wideBreak(ci) : shortBreak(ci);
    std::string out;
    out += brk;

    char buf[512];

    if (wide) {
        snprintf(buf, sizeof(buf), "%-33s %9s %9s %8s %9s %8s %10s %16s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code", "Complexity", "Complexity/Lines");
    } else if (!noComplexity) {
        snprintf(buf, sizeof(buf), "%-15s %9s %11s %9s %9s %10s %10s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code", "Complexity");
    } else {
        snprintf(buf, sizeof(buf), "%-21s %11s %11s %10s %11s %10s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code");
    }
    out += buf;
    if (!byFile) out += brk;

    for (auto& ls : langs) {
        if (byFile) out += brk;

        std::string name = ls.name;
        int nameW = wide ? 33 : (noComplexity ? 21 : 15);
        if ((int)name.size() > nameW) name = name.substr(0, nameW - 1) + "\xe2\x80\xa6";

        if (wide) {
            snprintf(buf, sizeof(buf), "%-33s %9ld %9ld %8ld %9ld %8ld %10ld %16.2f\n",
                     name.c_str(), ls.count, ls.lines, ls.blank, ls.comment, ls.code, ls.complexity, ls.weightedComplexity);
        } else if (!noComplexity) {
            snprintf(buf, sizeof(buf), "%-15s %9ld %11ld %9ld %9ld %10ld %10ld\n",
                     name.c_str(), ls.count, ls.lines, ls.blank, ls.comment, ls.code, ls.complexity);
        } else {
            snprintf(buf, sizeof(buf), "%-21s %11ld %11ld %10ld %11ld %10ld\n",
                     name.c_str(), ls.count, ls.lines, ls.blank, ls.comment, ls.code);
        }
        out += buf;

        if (byFile) {
            out += brk;
            for (auto* f : ls.files) {
                std::string loc = f->location;
                if (wide) {
                    if (loc.size() > 42) loc = "~" + loc.substr(loc.size() - 41);
                    snprintf(buf, sizeof(buf), "%-43s %9ld %8ld %9ld %8ld %10ld %16.2f\n",
                             loc.c_str(), f->lines, f->blank, f->comment, f->code, f->complexity, f->weightedComplexity);
                } else if (!noComplexity) {
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

    /* Total */
    out += brk;
    double totalWC = 0;
    for (auto& ls : langs) totalWC += ls.weightedComplexity;
    if (wide) {
        snprintf(buf, sizeof(buf), "%-33s %9ld %9ld %8ld %9ld %8ld %10ld %16.2f\n",
                 "Total", sf, sl, sb, sco, sc, sx, totalWC);
    } else if (!noComplexity) {
        snprintf(buf, sizeof(buf), "%-15s %9ld %11ld %9ld %9ld %10ld %10ld\n",
                 "Total", sf, sl, sb, sco, sc, sx);
    } else {
        snprintf(buf, sizeof(buf), "%-21s %11ld %11ld %10ld %11ld %10ld\n",
                 "Total", sf, sl, sb, sco, sc);
    }
    out += buf;
    out += brk;

    return out;
}

/* ---- JSON ---- */

std::string formatJSON(std::vector<FileJob*>& jobs, bool byFile) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, byFile, "files");

    std::string out = "[";
    for (size_t i = 0; i < langs.size(); i++) {
        auto& ls = langs[i];
        if (i > 0) out += ",";
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"Name\":\"%s\",\"Bytes\":%ld,\"CodeBytes\":0,\"Lines\":%ld,"
            "\"Code\":%ld,\"Comment\":%ld,\"Blank\":%ld,\"Complexity\":%ld,"
            "\"Count\":%ld,\"WeightedComplexity\":0,\"Files\":[]}",
            jsonEscape(ls.name).c_str(), ls.bytes, ls.lines,
            ls.code, ls.comment, ls.blank, ls.complexity, ls.count);
        out += buf;
    }
    out += "]";
    return out;
}

std::string formatJSON2(std::vector<FileJob*>& jobs) {
    std::string out = "[";
    for (size_t i = 0; i < jobs.size(); i++) {
        auto* f = jobs[i];
        if (i > 0) out += ",";
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "{\"Language\":\"%s\",\"Filename\":\"%s\",\"Location\":\"%s\","
            "\"Bytes\":%ld,\"Lines\":%ld,\"Code\":%ld,\"Comment\":%ld,"
            "\"Blank\":%ld,\"Complexity\":%ld,\"WeightedComplexity\":0}",
            jsonEscape(f->language).c_str(), jsonEscape(f->filename).c_str(),
            jsonEscape(f->location).c_str(), f->bytes, f->lines,
            f->code, f->comment, f->blank, f->complexity);
        out += buf;
    }
    out += "]";
    return out;
}

/* ---- CSV ---- */

std::string formatCSV(std::vector<FileJob*>& jobs, bool byFile) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, byFile, "files");

    std::string out = "Language,Files,Lines,Blanks,Comments,Code,Complexity,Bytes\n";
    char buf[512];
    for (auto& ls : langs) {
        snprintf(buf, sizeof(buf), "%s,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
                 ls.name.c_str(), ls.count, ls.lines, ls.blank,
                 ls.comment, ls.code, ls.complexity, ls.bytes);
        out += buf;
    }
    int64_t totalBytes = 0;
    for (auto& ls : langs) totalBytes += ls.bytes;
    snprintf(buf, sizeof(buf), "Total,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
             sf, sl, sb, sco, sc, sx, totalBytes);
    out += buf;
    return out;
}

/* ---- cloc-yaml ---- */

std::string formatClocYAML(std::vector<FileJob*>& jobs) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, false, "files");

    std::string out;
    char buf[256];
    snprintf(buf, sizeof(buf),
        "---\n"
        "header:\n"
        "  url: ''\n"
        "  version: ''\n"
        "  elapsed_seconds: 0\n"
        "  n_files: %ld\n"
        "  n_lines: %ld\n"
        "  files_per_second: 0\n"
        "  lines_per_second: 0\n",
        sf, sl);
    out += buf;

    for (auto& ls : langs) {
        out += ls.name + ":\n";
        snprintf(buf, sizeof(buf), "  code: %ld\n  comment: %ld\n  blank: %ld\n  nFiles: %ld\n",
                 ls.code, ls.comment, ls.blank, ls.count);
        out += buf;
    }

    snprintf(buf, sizeof(buf), "SUM:\n  code: %ld\n  comment: %ld\n  blank: %ld\n  nFiles: %ld\n",
             sc, sco, sb, sf);
    out += buf;
    return out;
}

/* ---- Format dispatch ---- */

std::string formatDispatch(const std::string& formatName,
                           std::vector<FileJob*>& jobs,
                           bool byFile, bool noComplexity,
                           bool wide, bool ci,
                           const std::string& sortBy) {
    if (icaseEq(formatName, "json"))
        return formatJSON(jobs, byFile);
    if (icaseEq(formatName, "json2"))
        return formatJSON2(jobs);
    if (icaseEq(formatName, "csv"))
        return formatCSV(jobs, byFile);
    if (icaseEq(formatName, "csv-stream"))
        return formatCSV(jobs, byFile); /* same output, stream just doesn't buffer */
    if (icaseEq(formatName, "cloc-yaml") || icaseEq(formatName, "cloc-yml"))
        return formatClocYAML(jobs);
    if (icaseEq(formatName, "wide"))
        return formatTabular(jobs, byFile, noComplexity, true, ci, sortBy);
    /* Default: tabular */
    return formatTabular(jobs, byFile, noComplexity, wide, ci, sortBy);
}

/* ---- Languages ---- */

std::string formatLanguages() {
    std::string out;
    std::vector<std::string> names;
    for (auto& [name, lang] : languageDatabase) names.push_back(name);
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
