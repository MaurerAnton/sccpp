#ifndef SCCPP_GITIGNORE_HPP
#define SCCPP_GITIGNORE_HPP

#include <string>
#include <vector>
#include <unordered_set>

/* Simple .gitignore pattern matcher.
   Supports: literal names, * wildcard, directory patterns (trailing /), negation (!),
   comments (#), and ** for recursive matching. */
class GitignoreMatcher {
public:
    GitignoreMatcher() = default;

    /* Load patterns from a .gitignore file. Returns number of patterns loaded. */
    int loadFile(const std::string& path);

    /* Add a single pattern line. */
    void addPattern(const std::string& line);

    /* Check if a path (relative to the ignore file's directory) should be ignored.
       isDir: true if the path is a directory. */
    bool isIgnored(const std::string& path, bool isDir) const;

    /* Clear all patterns. */
    void clear();

private:
    struct Pattern {
        std::string pattern;
        bool negated = false;   /* starts with ! */
        bool dirOnly = false;   /* ends with / */
        bool matchBase = false; /* pattern contains no / (matches anywhere) */
    };
    std::vector<Pattern> patterns_;

    bool matchPattern(const std::string& path, bool isDir, const Pattern& p) const;
    static bool matchGlob(const std::string& pattern, const std::string& str);
};

#endif
