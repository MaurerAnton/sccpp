#ifndef SCCPP_DETECTOR_HPP
#define SCCPP_DETECTOR_HPP

#include "common.hpp"
#include <string>
#include <vector>

/* Returns (languageNames, extension) */
std::pair<std::vector<std::string>, std::string> detectLanguage(const std::string& name);

/* Detect shebang language from content */
std::string detectSheBang(const std::string& content);

/* Determine the most likely language from possible options */
std::string determineLanguage(const std::string& filename,
                               const std::string& fallbackLanguage,
                               const std::vector<std::string>& possibleLanguages,
                               const std::vector<uint8_t>& content);

/* Extension cache helper */
std::string getExtension(const std::string& name);

/* Check file for remapping strings.
   Each pair is (pattern, language). First checks remapAll (applied to all files),
   then remapUnknown (applied only to unknown/shebang files).
   Returns true if language was remapped. */
bool hardRemapLanguage(FileJob* job,
                       const std::vector<std::pair<std::string, std::string>>& remapAll,
                       const std::vector<std::pair<std::string, std::string>>& remapUnknown);

#endif
