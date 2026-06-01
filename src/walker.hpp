#ifndef SCCPP_WALKER_HPP
#define SCCPP_WALKER_HPP

#include "common.hpp"
#include "gitignore.hpp"
#include <string>
#include <vector>

struct WalkResult {
    std::string path;
    std::string filename;
    int64_t size;
    bool isDir;
    bool isSymlink;
};

/* Walk options */
struct WalkOptions {
    std::vector<std::string> pathDenyList;
    std::vector<std::string> allowListExtensions;
    std::vector<std::string> excludeListExtensions;
    std::vector<std::string> excludeFilenames;
    bool includeSymLinks = false;
    bool noLarge = false;
    int64_t largeLineCount = 40000;
    int64_t largeByteCount = 1000000;
    bool noGitignore = false;
    bool noIgnore = false;
    bool noSccIgnore = false;
    bool noGitmodule = false;
    bool countIgnore = false;
};

void walkAndProcess(
    const std::vector<std::string>& dirFilePaths,
    const WalkOptions& opts,
    std::vector<FileJob*>& outJobs
);

std::vector<uint8_t> readFileContent(const std::string& path, int maxBytes);
bool isFileBinary(const std::string& path);

#endif
