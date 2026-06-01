#include "walker.hpp"
#include "detector.hpp"
#include "language.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

static bool pathInList(const std::string& path, const std::vector<std::string>& list) {
    /* Extract directory name from path */
    std::string dirName;
    size_t slash = path.rfind('/');
    if (slash != std::string::npos) dirName = path.substr(slash + 1);
    else dirName = path;
    for (auto& d : list) {
        if (dirName == d) return true;
    }
    return false;
}

static void walkDir(const std::string& dirPath,
                    const std::vector<std::string>& pathDenyList,
                    std::vector<WalkResult>& results) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        std::string fullPath = dirPath + "/" + entry->d_name;

        if (pathInList(entry->d_name, pathDenyList)) continue;

        struct stat st;
        if (lstat(fullPath.c_str(), &st) != 0) continue;

        WalkResult wr;
        wr.path = fullPath;
        wr.filename = entry->d_name;
        wr.size = st.st_size;
        wr.isDir = S_ISDIR(st.st_mode);
        wr.isSymlink = S_ISLNK(st.st_mode);

        results.push_back(wr);

        if (wr.isDir && !wr.isSymlink) {
            walkDir(fullPath, pathDenyList, results);
        }
    }
    closedir(dir);
}

std::vector<uint8_t> readFileContent(const std::string& path, int maxBytes) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return {};

    std::vector<uint8_t> buf;
    buf.resize(maxBytes > 0 ? maxBytes : 1048576);

    ssize_t total = 0;
    while (total < (ssize_t)buf.size()) {
        ssize_t n = read(fd, buf.data() + total, buf.size() - total);
        if (n <= 0) break;
        total += n;
    }
    close(fd);

    buf.resize(total);
    return buf;
}

bool isFileBinary(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    uint8_t buf[10000];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);

    if (n <= 0) return false;
    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] == 0) return true;
    }
    return false;
}

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
    std::vector<FileJob*>& outJobs) {

    /* Collect all walk results */
    std::vector<WalkResult> allResults;

    for (auto& p : dirFilePaths) {
        struct stat st;
        if (lstat(p.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walkDir(p, pathDenyList, allResults);
        } else {
            WalkResult wr;
            wr.path = p;
            wr.filename = [&p]() { auto s = p.rfind('/'); return (s != std::string::npos) ? p.substr(s+1) : p; }();
            wr.size = st.st_size;
            wr.isDir = false;
            wr.isSymlink = S_ISLNK(st.st_mode);
            allResults.push_back(wr);
        }
    }

    /* Process each result into FileJob */
    for (auto& wr : allResults) {
        if (wr.isDir) continue;

        /* Symlink handling */
        std::string realPath = wr.path;
        if (wr.isSymlink) {
            if (!includeSymLinks) continue;
            char buf[4096];
            ssize_t n = readlink(wr.path.c_str(), buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            realPath = buf;
            if (realPath[0] != '/') {
                /* Resolve relative */
                std::string dir = wr.path.substr(0, wr.path.rfind('/'));
                realPath = dir + "/" + realPath;
            }
            /* Update stat for real path */
            struct stat st2;
            if (stat(realPath.c_str(), &st2) != 0) continue;
            wr.size = st2.st_size;
        }

        /* Check regular file */
        struct stat rst;
        if (stat(realPath.c_str(), &rst) != 0) continue;
        if (!S_ISREG(rst.st_mode)) continue;

        /* Size check */
        if (noLarge && wr.size >= largeByteCount) continue;

        /* Language detection */
        auto [langs, ext] = detectLanguage(wr.filename);
        if (langs.empty()) continue;

        /* Extension filters */
        if (!allowListExtensions.empty()) {
            bool found = false;
            for (auto& ae : allowListExtensions) {
                if (ae == ext) { found = true; break; }
            }
            if (!found) continue;
        }

        if (!excludeListExtensions.empty()) {
            bool excluded = false;
            for (auto& ee : excludeListExtensions) {
                if (ee == ext) { excluded = true; break; }
            }
            if (excluded) continue;
        }

        if (!excludeFilename.empty()) {
            bool excluded = false;
            for (auto& ef : excludeFilename) {
                if (wr.filename.find(ef) != std::string::npos) { excluded = true; break; }
            }
            if (excluded) continue;
        }

        /* Build FileJob */
        FileJob* job = new FileJob();
        job->location = realPath;
        job->symlocation = wr.isSymlink ? wr.path : "";
        job->filename = wr.filename;
        job->extension = ext;
        job->possibleLanguages = langs;
        job->bytes = wr.size;

        outJobs.push_back(job);
    }
}
