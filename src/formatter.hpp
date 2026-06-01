#ifndef SCCPP_FORMATTER_HPP
#define SCCPP_FORMATTER_HPP

#include "common.hpp"
#include <string>
#include <vector>

struct FormatOptions {
    bool byFile = false;
    bool noComplexity = false;
    bool wide = false;
    bool ci = false;
    bool noHborder = false;
    bool uloc = false;
    bool dryness = false;
    bool percent = false;
    bool maxMean = false;
    std::string sortBy = "files";
    std::string formatName = "tabular";
};

/* Format file jobs into tabular output string */
std::string formatTabular(std::vector<FileJob*>& jobs, const FormatOptions& opts);

/* JSON output (array of language summaries) */
std::string formatJSON(std::vector<FileJob*>& jobs, const FormatOptions& opts);

/* JSON2 output (includes per-file data) */
std::string formatJSON2(std::vector<FileJob*>& jobs);

/* CSV output */
std::string formatCSV(std::vector<FileJob*>& jobs, const FormatOptions& opts);

/* cloc-yaml output */
std::string formatClocYAML(std::vector<FileJob*>& jobs);

/* Print supported languages */
std::string formatLanguages();

/* Dispatch to the right formatter by format name */
std::string formatDispatch(const FormatOptions& opts, std::vector<FileJob*>& jobs);

#endif
