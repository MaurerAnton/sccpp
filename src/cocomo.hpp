#ifndef SCCPP_COCOMO_HPP
#define SCCPP_COCOMO_HPP

#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>

/* Basic COCOMO model parameters (Boehm) */
extern std::unordered_map<std::string, std::vector<double>> cocomoProjectTypes;

/* ---- COCOMO ---- */
double estimateEffort(int64_t sloc, double eaf, const std::string& projectType);
double estimateScheduleMonths(double effortApplied, const std::string& projectType);
double estimateCost(double effortApplied, int64_t averageWage, double overhead);
std::string formatCocomo(int64_t sumCode, int64_t averageWage, double overhead,
                          double eaf, const std::string& projectType,
                          const std::string& currency, bool sloccount);

/* ---- LOCOMO (LLM Output COst MOdel) ---- */
struct LocomoPreset {
    std::string name;
    double inputPrice;   /* per 1M input tokens */
    double outputPrice;  /* per 1M output tokens */
    double tps;          /* output tokens per second */
};

struct LocomoResult {
    double inputTokens;
    double outputTokens;
    double cost;
    double generationSeconds;
    double reviewHours;
    double iterationFactor;
    std::string preset;
};

const LocomoPreset& getLocomoPreset(const std::string& name);
LocomoResult locomoEstimate(int64_t sumCode, int64_t sumComplexity,
                             const std::string& presetName,
                             double inputPriceOverride, double outputPriceOverride,
                             double tpsOverride, double reviewMinPerLine,
                             bool inputPriceSet, bool outputPriceSet, bool tpsSet,
                             double cyclesOverride, bool cyclesSet);
std::string formatLocomo(const LocomoResult& r, const std::string& currency);

/* ---- Size ---- */
std::string formatSize(int64_t sumBytes, const std::string& sizeUnit);

/* ---- Number formatting ---- */
std::string formatIntWithCommas(int64_t n);

#endif
