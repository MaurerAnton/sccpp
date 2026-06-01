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
#include <stack>

static bool pathInList(const std::string& name, const std::vector<std::string>& list) {
    for (auto& d : list) {
        if (name == d) return true;
    }
    return false;
}

struct DirContext {
    std::string path;
    GitignoreMatcher gitignore;
    GitignoreMatcher ignoreFile;
    GitignoreMatcher sccignore;
    bool loadedGitignore = false;
    bool loadedIgnore = false;
    bool loadedSccignore = false;
};

static void walkDir(const std::string& dirPath,
                    const WalkOptions& opts,
                    std::vector<WalkResult>& results,
                    DirContext* parentCtx = nullptr) {

    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    /* Build context for this directory */
    DirContext ctx;
    ctx.path = dirPath;

    /* Inherit parent's gitignore if we don't have our own */
    if (parentCtx) {
        ctx.gitignore = parentCtx->gitignore;
        ctx.ignoreFile = parentCtx->ignoreFile;
        ctx.sccignore = parentCtx->sccignore;
        ctx.loadedGitignore = parentCtx->loadedGitignore;
        ctx.loadedIgnore = parentCtx->loadedIgnore;
        ctx.loadedSccignore = parentCtx->loadedSccignore;
    }

    /* Try to load .gitignore from this directory */
    if (!opts.noGitignore && !ctx.loadedGitignore) {
        std::string giPath = dirPath + "/.gitignore";
        struct stat st;
        if (stat(giPath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            ctx.gitignore.loadFile(giPath);
            ctx.loadedGitignore = true;
        }
    }

    /* Try to load .ignore from this directory */
    if (!opts.noIgnore && !ctx.loadedIgnore) {
        std::string igPath = dirPath + "/.ignore";
        struct stat st;
        if (stat(igPath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            ctx.ignoreFile.loadFile(igPath);
            ctx.loadedIgnore = true;
        }
    }

    /* Try to load .sccignore from this directory */
    if (!opts.noSccIgnore && !ctx.loadedSccignore) {
        std::string scPath = dirPath + "/.sccignore";
        struct stat st;
        if (stat(scPath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            ctx.sccignore.loadFile(scPath);
            ctx.loadedSccignore = true;
        }
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        std::string name = entry->d_name;
        std::string fullPath = dirPath + "/" + name;

        struct stat st;
        if (lstat(fullPath.c_str(), &st) != 0) continue;

        bool isDir = S_ISDIR(st.st_mode);

        /* Check .gitignore patterns */
        std::string relPath = fullPath;
        /* Make path relative to where the gitignore was loaded */
        if (!ctx.gitignore.isIgnored(name, isDir) &&
            !ctx.ignoreFile.isIgnored(name, isDir) &&
            !ctx.sccignore.isIgnored(name, isDir)) {
            /* Not ignored by any ignore file */
        } else {
            continue; /* Skip ignored files/dirs */
        }

        /* Check path deny list */
        if (pathInList(name, opts.pathDenyList)) continue;

        WalkResult wr;
        wr.path = fullPath;
        wr.filename = name;
        wr.size = st.st_size;
        wr.isDir = isDir;
        wr.isSymlink = S_ISLNK(st.st_mode);

        if (!isDir) {
            results.push_back(wr);
        }

        if (isDir && !wr.isSymlink) {
            walkDir(fullPath, opts, results, &ctx);
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
    const WalkOptions& opts,
    std::vector<FileJob*>& outJobs) {

    /* Collect all walk results */
    std::vector<WalkResult> allResults;

    for (auto& p : dirFilePaths) {
        struct stat st;
        if (lstat(p.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walkDir(p, opts, allResults);
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
            if (!opts.includeSymLinks) continue;
            char buf[4096];
            ssize_t n = readlink(wr.path.c_str(), buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            realPath = buf;
            if (realPath[0] != '/') {
                std::string dir = wr.path.substr(0, wr.path.rfind('/'));
                realPath = dir + "/" + realPath;
            }
            struct stat st2;
            if (stat(realPath.c_str(), &st2) != 0) continue;
            wr.size = st2.st_size;
        }

        /* Check regular file */
        struct stat rst;
        if (stat(realPath.c_str(), &rst) != 0) continue;
        if (!S_ISREG(rst.st_mode)) continue;

        /* Size check */
        if (opts.noLarge && wr.size >= opts.largeByteCount) continue;

        /* Language detection */
        auto [langs, ext] = detectLanguage(wr.filename);
        if (langs.empty()) continue;

        /* Extension filters */
        if (!opts.allowListExtensions.empty()) {
            bool found = false;
            for (auto& ae : opts.allowListExtensions)
                if (ae == ext) { found = true; break; }
            if (!found) continue;
        }

        if (!opts.excludeListExtensions.empty()) {
            bool excluded = false;
            for (auto& ee : opts.excludeListExtensions)
                if (ee == ext) { excluded = true; break; }
            if (excluded) continue;
        }

        if (!opts.excludeFilenames.empty()) {
            bool excluded = false;
            for (auto& ef : opts.excludeFilenames)
                if (wr.filename.find(ef) != std::string::npos) { excluded = true; break; }
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

        /* Skip ignore/gitignore files unless --count-ignore */
        if (!opts.countIgnore) {
            for (auto& l : langs) {
                if (l == "ignore" || l == "gitignore") {
                    delete job;
                    job = nullptr;
                    break;
                }
            }
        }
        if (job) outJobs.push_back(job);
    }
}
