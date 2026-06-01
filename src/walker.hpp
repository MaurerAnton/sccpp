#ifndef SCCPP_WALKER_HPP
#define SCCPP_WALKER_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>

/* File walker that traverses directories and finds code files */
struct WalkResult {
    std::string path;
    std::string filename;
    int64_t size;
    bool isDir;
    bool isSymlink;
};

/* Process all paths (files and dirs), push FileJobs to output queue */
void walkAndProcess(
    const std::vector<std::string>& dirFilePaths,
    const std::vector<std::string>& pathDenyList,
    const std::vector<std::string>& allowListExtensions,
    const std::vector<std::string>& excludeListExtensions,
    const std::vector<std::string>& excludeFilename,
    bool includeSymLinks,
    bool noLarge,
    int64_t largeLineCount,
    int64_t largeByteCount,
    std::vector<FileJob*>& outJobs
);

/* Check if a file is binary by looking for NUL in first 10000 bytes */
bool isFileBinary(const std::string& path);

/* Read file content */
std::vector<uint8_t> readFileContent(const std::string& path, int maxBytes);

#endif
