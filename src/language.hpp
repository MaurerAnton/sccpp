#ifndef SCCPP_LANGUAGE_HPP
#define SCCPP_LANGUAGE_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

/* Global language database - loaded at startup */
extern std::unordered_map<std::string, Language> languageDatabase;
extern std::unordered_map<std::string, std::vector<std::string>> extensionToLanguage;
extern std::unordered_map<std::string, std::string> filenameToLanguage;
extern std::unordered_map<std::string, std::vector<std::string>> shebangLookup;
extern std::unordered_map<std::string, LanguageFeature> languageFeatures;
extern std::mutex languageFeaturesMutex;

/* Initialise the language database from the embedded JSON */
void initLanguageDatabase();

/* Load/build a LanguageFeature for a specific language name */
void loadLanguageFeature(const std::string& name);

/* Process a raw Language into a LanguageFeature */
void processLanguageFeature(const std::string& name, const Language& lang, LanguageFeature& feat);

/* Setup count-as mappings */
void setupCountAs(const std::string& countAs);

#endif
