#ifndef SCCPP_COCOMO_HPP
#define SCCPP_COCOMO_HPP

#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>

/* Basic COCOMO model parameters (Boehm) */
extern std::unordered_map<std::string, std::vector<double>> cocomoProjectTypes;

/* COCOMO calculation functions */
double estimateEffort(int64_t sloc, double eaf, const std::string& projectType);
double estimateScheduleMonths(double effortApplied, const std::string& projectType);
double estimateCost(double effortApplied, int64_t averageWage, double overhead);

/* Format COCOMO output */
std::string formatCocomo(int64_t sumCode, int64_t averageWage, double overhead,
                          double eaf, const std::string& projectType,
                          const std::string& currency, bool sloccount);

/* Format size output */
std::string formatSize(int64_t sumBytes, const std::string& sizeUnit);

#endif
