#include "language.hpp"
#include "detector.hpp"
#include "counter.hpp"
#include "walker.hpp"
#include "formatter.hpp"
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm>
#include <atomic>

/* ---- Flags (matching scc CLI) ---- */
static bool flagLanguages = false;
static bool flagByFile = false;
static bool flagNoComplexity = false;   /* --no-complexity / -c */
static bool flagMore = false;           /* --wide / -w */
static bool flagCi = false;            /* --ci */
static bool flagIncludeSymLinks = false;
static bool flagNoLarge = false;
static int64_t flagLargeLineCount = 40000;
static int64_t flagLargeByteCount = 1000000;
static std::string flagSortBy = "files";
static std::string flagFormat = "tabular";
static std::string flagAllowExt;
static std::string flagExcludeExt;
static std::string flagExcludeFile;
static std::string flagOutput;
static int flagWorkers = 0;

static void printUsage(const char* prog) {
    fprintf(stderr,
        "sccpp - Sloc Cloc and Code (pure C++ port of scc)\n"
        "Usage: %s [flags] [files or directories...]\n\n"
        "Flags:\n"
        "  -f, --format FMT    Output format: tabular (default)\n"
        "  -l, --languages     Print supported languages and exit\n"
        "  -c, --no-complexity Skip complexity calculation\n"
        "  -w, --wide          Wider output\n"
        "  --by-file           Show per-file output\n"
        "  --ci                ASCII-safe output for CI\n"
        "  --include-symlinks  Count symlinked files\n"
        "  --no-large          Skip large files\n"
        "  --exclude-dir DIR   Directories to exclude (repeatable)\n"
        "  -i, --include-ext E Limit to extensions (comma-sep)\n"
        "  -x, --exclude-ext E Exclude extensions (comma-sep)\n"
        "  -s, --sort COL      Sort by: files, lines, blanks, code, comments, complexity\n"
        "  -o, --output FILE   Write output to file\n"
        "  -j, --workers N     Number of worker threads (default: CPU count)\n"
        "  -h, --help          Show this help\n",
        prog);
}

static std::vector<std::string> splitCSV(const std::string& s) {
    std::vector<std::string> result;
    if (s.empty()) return result;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t comma = s.find(',', pos);
        std::string item = (comma == std::string::npos) ? s.substr(pos) : s.substr(pos, comma - pos);
        if (!item.empty()) result.push_back(item);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return result;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> paths;
    std::vector<std::string> excludeDirs;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { printUsage(argv[0]); return 0; }
        else if (arg == "-l" || arg == "--languages") flagLanguages = true;
        else if (arg == "-c" || arg == "--no-complexity") flagNoComplexity = true;
        else if (arg == "-w" || arg == "--wide") flagMore = true;
        else if (arg == "--by-file") flagByFile = true;
        else if (arg == "--ci") flagCi = true;
        else if (arg == "--include-symlinks") flagIncludeSymLinks = true;
        else if (arg == "--no-large") flagNoLarge = true;
        else if ((arg == "-f" || arg == "--format") && i + 1 < argc) flagFormat = argv[++i];
        else if ((arg == "-s" || arg == "--sort") && i + 1 < argc) flagSortBy = argv[++i];
        else if ((arg == "-i" || arg == "--include-ext") && i + 1 < argc) flagAllowExt = argv[++i];
        else if ((arg == "-x" || arg == "--exclude-ext") && i + 1 < argc) flagExcludeExt = argv[++i];
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) flagOutput = argv[++i];
        else if ((arg == "-j" || arg == "--workers") && i + 1 < argc) flagWorkers = std::atoi(argv[++i]);
        else if (arg == "--exclude-dir" && i + 1 < argc) excludeDirs.push_back(argv[++i]);
        else if (arg[0] != '-') paths.push_back(arg);
        else { fprintf(stderr, "Unknown flag: %s\n", arg.c_str()); return 1; }
    }

    if (excludeDirs.empty()) excludeDirs = {".git", ".hg", ".svn"};

    initLanguageDatabase();

    if (flagLanguages) {
        fprintf(stdout, "%s", formatLanguages().c_str());
        return 0;
    }

    /* Pre-load all language features */
    languageFeatures.reserve(languageDatabase.size() * 2);
    for (auto& [name, lang] : languageDatabase) loadLanguageFeature(name);

    if (paths.empty()) paths.push_back(".");

    auto allowExts = splitCSV(flagAllowExt);
    auto excludeExts = splitCSV(flagExcludeExt);
    std::vector<std::string> excludeFns;

    /* Walk and collect jobs */
    std::vector<FileJob*> jobs;
    walkAndProcess(paths, excludeDirs, allowExts, excludeExts, excludeFns,
                   flagIncludeSymLinks, flagNoLarge,
                   flagLargeLineCount, flagLargeByteCount, jobs);

    if (jobs.empty()) {
        fprintf(stdout, "No files found to process\n");
        return 0;
    }

    int numWorkers = flagWorkers > 0 ? flagWorkers : (int)std::thread::hardware_concurrency();
    if (numWorkers < 1) numWorkers = 1;

    /* Process files in parallel */
    std::atomic<size_t> nextIdx{0};
    std::vector<std::thread> workers;

    for (int t = 0; t < numWorkers; t++) {
        workers.emplace_back([&]() {
            while (true) {
                size_t idx = nextIdx.fetch_add(1);
                if (idx >= jobs.size()) break;

                FileJob* job = jobs[idx];
                job->content = readFileContent(
                    job->symlocation.empty() ? job->location : job->symlocation,
                    (int)job->bytes);

                if (job->content.empty()) { job->lines = 0; continue; }

                if (job->language == SheBang) {
                    std::string sb = detectSheBang(
                        std::string(reinterpret_cast<const char*>(job->content.data()),
                                    std::min(job->content.size(), (size_t)200)));
                    if (!sb.empty()) { job->language = sb; loadLanguageFeature(sb); }
                    else continue;
                }

                if (job->possibleLanguages.size() > 1)
                    job->language = determineLanguage(job->filename, job->language,
                                                       job->possibleLanguages, job->content);
                else if (job->possibleLanguages.size() == 1)
                    job->language = job->possibleLanguages[0];

                countStats(job, flagNoComplexity);
            }
        });
    }
    for (auto& w : workers) w.join();

    /* Format output */
    std::string output = formatDispatch(flagFormat, jobs, flagByFile, flagNoComplexity,
                                         flagMore, flagCi, flagSortBy);

    if (flagOutput.empty()) {
        fprintf(stdout, "%s", output.c_str());
    } else {
        FILE* f = fopen(flagOutput.c_str(), "w");
        if (f) { fputs(output.c_str(), f); fclose(f); }
        else fprintf(stderr, "Could not write to %s\n", flagOutput.c_str());
    }

    for (auto* j : jobs) delete j;
    return 0;
}
