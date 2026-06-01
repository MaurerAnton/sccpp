#ifndef SCCPP_FORMATTER_HPP
#define SCCPP_FORMATTER_HPP

#include "common.hpp"
#include <string>
#include <vector>

/* Format file jobs into tabular output string */
std::string formatTabular(std::vector<FileJob*>& jobs, bool byFile,
                          bool complexity, bool more, bool ci,
                          bool noCocomo, bool noSize,
                          const std::string& sortBy);

/* Print supported languages */
std::string formatLanguages();

#endif
