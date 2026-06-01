#include "formatter.hpp"
#include "language.hpp"
#include "counter.hpp"
#include "cocomo.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <sstream>

/* ---- Helpers ---- */

static std::string shortBreak(bool ci, bool noHborder) {
    if (noHborder) return "";
    return ci
        ? "-------------------------------------------------------------------------------\n"
        : "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n";
}

static std::string shortBreakCi = "-------------------------------------------------------------------------------\n";

static bool icaseEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

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
                      int64_t& sumBytes, const FormatOptions& opts) {

    std::unordered_map<std::string, LanguageSummary> langs;
    sumFiles = sumLines = sumCode = sumComment = sumBlank = sumComplexity = sumBytes = 0;

    for (auto* job : jobs) {
        sumFiles++; sumLines += job->lines; sumCode += job->code;
        sumComment += job->comment; sumBlank += job->blank; sumComplexity += job->complexity;
        sumBytes += job->bytes;

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
            if (opts.byFile) ls.files.push_back(job);
        } else {
            ls.bytes += job->bytes; ls.lines += job->lines; ls.code += job->code;
            ls.comment += job->comment; ls.blank += job->blank;
            ls.complexity += job->complexity;
            ls.weightedComplexity += wc;
            ls.count++;
            if (opts.byFile) ls.files.push_back(job);
        }
    }

    for (auto& [name, ls] : langs) {
        /* Add ULOC from global state */
        auto it = ulocLanguageCount.find(ls.name);
        if (it != ulocLanguageCount.end()) ls.uloc = (int)it->second.size();
        sortedLangs.push_back(ls);
    }

    auto& sb = opts.sortBy;
    auto cmp = [&](const LanguageSummary& a, const LanguageSummary& b) {
        if (icaseEq(sb, "name") || icaseEq(sb, "names")) return a.name < b.name;
        if (icaseEq(sb, "line") || icaseEq(sb, "lines")) { if (a.lines != b.lines) return a.lines > b.lines; return a.name < b.name; }
        if (icaseEq(sb, "blank") || icaseEq(sb, "blanks")) { if (a.blank != b.blank) return a.blank > b.blank; return a.name < b.name; }
        if (icaseEq(sb, "code") || icaseEq(sb, "codes")) { if (a.code != b.code) return a.code > b.code; return a.name < b.name; }
        if (icaseEq(sb, "comment") || icaseEq(sb, "comments")) { if (a.comment != b.comment) return a.comment > b.comment; return a.name < b.name; }
        if (icaseEq(sb, "complexity") || icaseEq(sb, "comp")) { if (a.complexity != b.complexity) return a.complexity > b.complexity; return a.name < b.name; }
        if (icaseEq(sb, "byte") || icaseEq(sb, "bytes")) { if (a.bytes != b.bytes) return a.bytes > b.bytes; return a.name < b.name; }
        if (a.count != b.count) return a.count > b.count;
        return a.name < b.name;
    };
    std::sort(sortedLangs.begin(), sortedLangs.end(), cmp);

    if (opts.byFile) {
        for (auto& ls : sortedLangs) {
            std::sort(ls.files.begin(), ls.files.end(), [](FileJob* a, FileJob* b) {
                return a->lines > b->lines;
            });
        }
    }
}

static int maxIn(const std::vector<int>& v) {
    if (v.empty()) return 0;
    return *std::max_element(v.begin(), v.end());
}
static int meanIn(const std::vector<int>& v) {
    if (v.empty()) return 0;
    int64_t sum = 0;
    for (int x : v) sum += x;
    return (int)(sum / (int64_t)v.size());
}

/* ---- Tabular ---- */

std::string formatTabular(std::vector<FileJob*>& jobs, const FormatOptions& opts) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx, sbytes;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, sbytes, opts);

    std::string brk = shortBreak(opts.ci, opts.noHborder);
    std::string out;
    out += brk;

    char buf[512];

    if (opts.wide) {
        snprintf(buf, sizeof(buf), "%-33s %9s %9s %8s %9s %8s %10s %16s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code", "Complexity", "Complexity/Lines");
    } else if (!opts.noComplexity) {
        snprintf(buf, sizeof(buf), "%-15s %9s %11s %9s %9s %10s %10s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code", "Complexity");
    } else {
        snprintf(buf, sizeof(buf), "%-21s %11s %11s %10s %11s %10s\n",
                 "Language", "Files", "Lines", "Blanks", "Comments", "Code");
    }
    out += buf;
    if (!opts.byFile) out += brk;

    for (auto& ls : langs) {
        if (opts.byFile) out += brk;

        std::string name = ls.name;
        int nameW = opts.wide ? 33 : (opts.noComplexity ? 21 : 15);
        if ((int)name.size() > nameW) name = name.substr(0, nameW - 1) + "\xe2\x80\xa6";

        if (opts.wide) {
            snprintf(buf, sizeof(buf), "%-33s %9ld %9ld %8ld %9ld %8ld %10ld %16.2f\n",
                     name.c_str(), ls.count, ls.lines, ls.blank, ls.comment, ls.code, ls.complexity, ls.weightedComplexity);
        } else if (!opts.noComplexity) {
            snprintf(buf, sizeof(buf), "%-15s %9ld %11ld %9ld %9ld %10ld %10ld\n",
                     name.c_str(), ls.count, ls.lines, ls.blank, ls.comment, ls.code, ls.complexity);
        } else {
            snprintf(buf, sizeof(buf), "%-21s %11ld %11ld %10ld %11ld %10ld\n",
                     name.c_str(), ls.count, ls.lines, ls.blank, ls.comment, ls.code);
        }
        out += buf;

        /* Percent row */
        if (opts.percent) {
            if (!opts.noComplexity)
                snprintf(buf, sizeof(buf), "Percentage %13.1f%% %10.1f%% %8.1f%% %8.1f%% %9.1f%% %9.1f%%\n",
                    100.0*ls.count/sf, 100.0*ls.lines/sl, 100.0*ls.blank/sb, 100.0*ls.comment/sco, 100.0*ls.code/sc, 100.0*ls.complexity/sx);
            else
                snprintf(buf, sizeof(buf), "Percentage %21.1f%% %10.1f%% %8.1f%% %9.1f%% %9.1f%%\n",
                    100.0*ls.count/sf, 100.0*ls.lines/sl, 100.0*ls.blank/sb, 100.0*ls.comment/sco, 100.0*ls.code/sc);
            out += buf;
        }

        /* Max/Mean line length */
        if (opts.maxMean) {
            /* Collect line lengths from files */
            std::vector<int> llens;
            for (auto* f : ls.files) llens.insert(llens.end(), f->lineLength.begin(), f->lineLength.end());
            int mx = maxIn(llens), mn = meanIn(llens);
            if (!opts.noComplexity)
                snprintf(buf, sizeof(buf), "MaxLine / MeanLine %6d %11d\n", mx, mn);
            else
                snprintf(buf, sizeof(buf), "MaxLine / MeanLine %14d %11d\n", mx, mn);
            out += buf;
        }

        /* ULOC per language */
        if (opts.uloc) {
            if (!opts.noComplexity)
                snprintf(buf, sizeof(buf), "(ULOC) %30d\n", ls.uloc);
            else
                snprintf(buf, sizeof(buf), "(ULOC) %38d\n", ls.uloc);
            out += buf;
        }

        if (opts.byFile) {
            out += brk;
            for (auto* f : ls.files) {
                std::string loc = f->location;
                if (opts.wide) {
                    if (loc.size() > 42) loc = "~" + loc.substr(loc.size() - 41);
                    snprintf(buf, sizeof(buf), "%-43s %9ld %8ld %9ld %8ld %10ld %16.2f\n",
                             loc.c_str(), f->lines, f->blank, f->comment, f->code, f->complexity, f->weightedComplexity);
                } else if (!opts.noComplexity) {
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

        /* Separator after percent/uloc/maxmean rows */
        if ((opts.percent || opts.maxMean || opts.uloc) && !opts.byFile && &ls != &langs.back()) {
            out += shortBreakCi;
        }
    }

    /* Total row */
    out += brk;
    double totalWC = 0;
    for (auto& ls : langs) totalWC += ls.weightedComplexity;
    if (opts.wide) {
        snprintf(buf, sizeof(buf), "%-33s %9ld %9ld %8ld %9ld %8ld %10ld %16.2f\n",
                 "Total", sf, sl, sb, sco, sc, sx, totalWC);
    } else if (!opts.noComplexity) {
        snprintf(buf, sizeof(buf), "%-15s %9ld %11ld %9ld %9ld %10ld %10ld\n",
                 "Total", sf, sl, sb, sco, sc, sx);
    } else {
        snprintf(buf, sizeof(buf), "%-21s %11ld %11ld %10ld %11ld %10ld\n",
                 "Total", sf, sl, sb, sco, sc);
    }
    out += buf;
    out += brk;

    /* Global ULOC */
    if (opts.uloc) {
        if (!opts.noComplexity)
            snprintf(buf, sizeof(buf), "Unique Lines of Code (ULOC) %9d\n", (int)ulocGlobalCount.size());
        else
            snprintf(buf, sizeof(buf), "Unique Lines of Code (ULOC) %9d\n", (int)ulocGlobalCount.size());
        out += buf;
        if (opts.dryness && sl > 0) {
            double dry = 100.0 * ulocGlobalCount.size() / (double)sl;
            if (!opts.noComplexity)
                snprintf(buf, sizeof(buf), "DRYness %% %27.2f\n", dry);
            else
                snprintf(buf, sizeof(buf), "DRYness %% %27.2f\n", dry);
            out += buf;
        }
        out += brk;
    }

    /* COCOMO */
    if (opts.cocomo) {
        out += formatCocomo(sc, opts.averageWage, opts.overhead, opts.eaf,
                            opts.projectType, opts.currencySymbol, opts.sloccountFormat);
        out += brk;
    }

    /* LOCOMO */
    if (opts.locomo) {
        auto lr = locomoEstimate(sc, sx, opts.projectType,
                                  0, 0, 0, 0.01,
                                  false, false, false, 0, false);
        out += formatLocomo(lr, opts.currencySymbol);
        out += brk;
    }

    /* Size */
    if (!opts.noSize) {
        out += formatSize(sbytes, opts.sizeUnit);
        out += brk;
    }

    return out;
}

/* ---- JSON ---- */

std::string formatJSON(std::vector<FileJob*>& jobs, const FormatOptions& opts) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx, sbytes;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, sbytes, opts);

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

std::string formatCSV(std::vector<FileJob*>& jobs, const FormatOptions& opts) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx, sbytes;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, sbytes, opts);

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
    int64_t sf, sl, sc, sco, sb, sx, sbytes;
    FormatOptions defOpts;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, sbytes, defOpts);

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

static std::string htmlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

static std::string formatHtml(std::vector<FileJob*>& jobs, const FormatOptions& opts, bool fullPage) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx, sbytes;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, sbytes, opts);

    std::string out;
    if (fullPage) {
        out += "<html lang=\"en\"><head><meta charset=\"utf-8\" /><title>scc html output</title>"
               "<style>table{border-collapse:collapse}td,th{border:1px solid #999;padding:.5rem}</style></head><body>\n";
    }
    out += "<table id=\"scc-table\">\n<thead><tr>\n"
           "<th>Language</th><th>Files</th><th>Lines</th><th>Blank</th>"
           "<th>Comment</th><th>Code</th><th>Complexity</th><th>Bytes</th></tr></thead>\n<tbody>\n";

    char buf[512];
    for (auto& r : langs) {
        snprintf(buf, sizeof(buf),
            "<tr><th>%s</th><td>%ld</td><td>%ld</td><td>%ld</td>"
            "<td>%ld</td><td>%ld</td><td>%ld</td><td>%ld</td></tr>\n",
            htmlEscape(r.name).c_str(), r.count, r.lines, r.blank,
            r.comment, r.code, r.complexity, r.bytes);
        out += buf;
    }

    snprintf(buf, sizeof(buf),
        "</tbody>\n<tfoot><tr><th>Total</th><td>%ld</td><td>%ld</td>"
        "<td>%ld</td><td>%ld</td><td>%ld</td><td>%ld</td><td>%ld</td></tr>\n</tfoot></table>\n",
        sf, sl, sb, sco, sc, sx, [&](){ int64_t tb=0; for(auto&l:langs)tb+=l.bytes; return tb; }());
    out += buf;

    if (fullPage) out += "</body></html>\n";
    return out;
}

static std::string formatOpenMetrics(std::vector<FileJob*>& jobs, const FormatOptions& opts) {
    std::vector<LanguageSummary> langs;
    int64_t sf, sl, sc, sco, sb, sx, sbytes;
    aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, sbytes, opts);

    std::string out =
        "# TYPE scc_files gauge\n# HELP scc_files Number of sourcecode files.\n"
        "# TYPE scc_lines gauge\n# HELP scc_lines Number of lines.\n"
        "# TYPE scc_code gauge\n# HELP scc_code Number of lines of actual code.\n"
        "# TYPE scc_comments gauge\n# HELP scc_comments Number of comments.\n"
        "# TYPE scc_blanks gauge\n# HELP scc_blanks Number of blank lines.\n"
        "# TYPE scc_complexity gauge\n# HELP scc_complexity Code complexity.\n"
        "# TYPE scc_bytes gauge\n# UNIT scc_bytes bytes\n# HELP scc_bytes Size in bytes.\n";

    char buf[512];
    for (auto& r : langs) {
        snprintf(buf, sizeof(buf), "scc_files{language=\"%s\"} %ld\n", r.name.c_str(), r.count); out += buf;
        snprintf(buf, sizeof(buf), "scc_lines{language=\"%s\"} %ld\n", r.name.c_str(), r.lines); out += buf;
        snprintf(buf, sizeof(buf), "scc_code{language=\"%s\"} %ld\n", r.name.c_str(), r.code); out += buf;
        snprintf(buf, sizeof(buf), "scc_comments{language=\"%s\"} %ld\n", r.name.c_str(), r.comment); out += buf;
        snprintf(buf, sizeof(buf), "scc_blanks{language=\"%s\"} %ld\n", r.name.c_str(), r.blank); out += buf;
        snprintf(buf, sizeof(buf), "scc_complexity{language=\"%s\"} %ld\n", r.name.c_str(), r.complexity); out += buf;
        snprintf(buf, sizeof(buf), "scc_bytes{language=\"%s\"} %ld\n", r.name.c_str(), r.bytes); out += buf;
    }
    out += "# EOF\n";
    return out;
}

std::string formatDispatch(const FormatOptions& opts, std::vector<FileJob*>& jobs) {
    const auto& fn = opts.formatName;
    if (icaseEq(fn, "json"))  return formatJSON(jobs, opts);
    if (icaseEq(fn, "json2")) return formatJSON2(jobs);
    if (icaseEq(fn, "csv") || icaseEq(fn, "csv-stream")) return formatCSV(jobs, opts);
    if (icaseEq(fn, "cloc-yaml") || icaseEq(fn, "cloc-yml")) return formatClocYAML(jobs);
    if (icaseEq(fn, "html")) return formatHtml(jobs, opts, true);
    if (icaseEq(fn, "html-table")) return formatHtml(jobs, opts, false);
    if (icaseEq(fn, "openmetrics")) return formatOpenMetrics(jobs, opts);
    if (icaseEq(fn, "sql") || icaseEq(fn, "sql-insert")) {
        /* SQL INSERT format */
        std::vector<LanguageSummary> langs;
        int64_t sf, sl, sc, sco, sb, sx, sbytes;
        aggregate(jobs, langs, sf, sl, sc, sco, sb, sx, sbytes, opts);
        std::string out;
        char buf[512];
        for (auto& r : langs) {
            snprintf(buf, sizeof(buf),
                "INSERT INTO scc (language,files,lines,blanks,comments,code,complexity,bytes) "
                "VALUES ('%s',%ld,%ld,%ld,%ld,%ld,%ld,%ld);\n",
                r.name.c_str(), r.count, r.lines, r.blank, r.comment, r.code, r.complexity, r.bytes);
            out += buf;
        }
        return out;
    }
    return formatTabular(jobs, opts);
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
