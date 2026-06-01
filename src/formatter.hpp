#ifndef SCCPP_FORMATTER_HPP
#define SCCPP_FORMATTER_HPP

#include "common.hpp"
#include <string>
#include <vector>

/* Format file jobs into tabular output string */
std::string formatTabular(std::vector<FileJob*>& jobs, bool byFile,
                          bool noComplexity, bool wide, bool ci,
                          const std::string& sortBy);

/* JSON output (array of language summaries) */
std::string formatJSON(std::vector<FileJob*>& jobs, bool byFile);

/* JSON2 output (includes per-file data) */
std::string formatJSON2(std::vector<FileJob*>& jobs);

/* CSV output */
std::string formatCSV(std::vector<FileJob*>& jobs, bool byFile);

/* cloc-yaml output */
std::string formatClocYAML(std::vector<FileJob*>& jobs);

/* Print supported languages */
std::string formatLanguages();

/* Dispatch to the right formatter by format name */
std::string formatDispatch(const std::string& formatName,
                           std::vector<FileJob*>& jobs,
                           bool byFile, bool noComplexity,
                           bool wide, bool ci,
                           const std::string& sortBy);

#endif
