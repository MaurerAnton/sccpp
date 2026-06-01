#ifndef SCCPP_COUNTER_HPP
#define SCCPP_COUNTER_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

/* Processing options passed to CountStats */
struct CountOptions {
    bool skipComplexity = false;
    bool checkMinified = false;
    bool checkGenerated = false;
    bool checkBinary = true;       /* --binary=false disables NUL check */
    int  minGenLineLength = 255;
    std::vector<std::string> generatedMarkers;
};

/* CountStats - the core state machine that processes file content. */
void countStats(FileJob* job, const CountOptions& opts);

/* Global ULOC state (mutex-protected) */
extern std::mutex ulocMutex;
extern std::unordered_set<std::string> ulocGlobalCount;
extern std::unordered_map<std::string, std::unordered_set<std::string>> ulocLanguageCount;

/* Called after countStats to track ULOC */
void trackUloc(FileJob* job);

/* Simple 64-bit hash for duplicate detection */
uint64_t hashContent(const std::vector<uint8_t>& data);

#endif
