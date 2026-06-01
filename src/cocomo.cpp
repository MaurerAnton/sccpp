#include "cocomo.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <sstream>

std::unordered_map<std::string, std::vector<double>> cocomoProjectTypes = {
    {"organic",       {2.4, 1.05, 2.5, 0.38}},
    {"semi-detached", {3.0, 1.12, 2.5, 0.35}},
    {"embedded",      {3.6, 1.20, 2.5, 0.32}},
};

/* ---- Number formatting ---- */

std::string formatIntWithCommas(int64_t n) {
    if (n < 0) return "-" + formatIntWithCommas(-n);
    std::string s = std::to_string(n);
    int len = (int)s.size();
    for (int i = len - 3; i > 0; i -= 3)
        s.insert(i, ",");
    return s;
}

/* ---- COCOMO ---- */

double estimateEffort(int64_t sloc, double eaf, const std::string& projectType) {
    auto it = cocomoProjectTypes.find(projectType);
    if (it == cocomoProjectTypes.end()) it = cocomoProjectTypes.find("organic");
    auto& p = it->second;
    return p[0] * std::pow((double)sloc / 1000.0, p[1]) * eaf;
}

double estimateScheduleMonths(double effortApplied, const std::string& projectType) {
    auto it = cocomoProjectTypes.find(projectType);
    if (it == cocomoProjectTypes.end()) it = cocomoProjectTypes.find("organic");
    auto& p = it->second;
    return p[2] * std::pow(effortApplied, p[3]);
}

double estimateCost(double effortApplied, int64_t averageWage, double overhead) {
    return effortApplied * (double)(averageWage / 12) * overhead;
}

static bool isLeapYear(int year) {
    if (year % 4 != 0) return false;
    if (year % 100 != 0) return true;
    return year % 400 == 0;
}

std::string formatCocomo(int64_t sumCode, int64_t averageWage, double overhead,
                          double eaf, const std::string& projectType,
                          const std::string& currency, bool sloccount) {
    double effort = estimateEffort(sumCode, eaf, projectType);
    double schedMonths = estimateScheduleMonths(effort, projectType);
    double cost = estimateCost(effort, averageWage, overhead);
    double people = (schedMonths > 0) ? effort / schedMonths : 0.0;

    char buf[512];
    std::string out;

    if (sloccount) {
        auto& p = cocomoProjectTypes[projectType.empty() ? "organic" : projectType];
        snprintf(buf, sizeof(buf),
            "Total Physical Source Lines of Code (SLOC)                     = %s\n"
            "Development Effort Estimate, Person-Years (Person-Months)      = %.2f (%.2f)\n"
            " (Basic COCOMO model, Person-Months = %.2f*(KSLOC**%.2f)*%.2f)\n"
            "Schedule Estimate, Years (Months)                              = %.2f (%.2f)\n"
            " (Basic COCOMO model, Months = %.2f*(person-months**%.2f))\n"
            "Estimated Average Number of Developers (Effort/Schedule)       = %.2f\n"
            "Total Estimated Cost to Develop                                = %s%s\n"
            " (average salary = %s%s/year, overhead = %.2f)\n",
            formatIntWithCommas(sumCode).c_str(),
            effort / 12, effort,
            p[0], p[1], eaf,
            schedMonths / 12, schedMonths,
            p[2], p[3],
            people,
            currency.c_str(), formatIntWithCommas((int64_t)cost).c_str(),
            currency.c_str(), formatIntWithCommas(averageWage).c_str(), overhead);
    } else {
        snprintf(buf, sizeof(buf),
            "Estimated Cost to Develop (%s) %s%s\n"
            "Estimated Schedule Effort (%s) %.2f months\n"
            "Estimated People Required (%s) %.2f\n",
            projectType.c_str(), currency.c_str(), formatIntWithCommas((int64_t)cost).c_str(),
            projectType.c_str(), schedMonths,
            projectType.c_str(), people);
    }
    out = buf;
    return out;
}

/* ---- LOCOMO ---- */

static std::unordered_map<std::string, LocomoPreset> locomoPresets = {
    {"large",  {"large",  10.00, 30.00, 30}},
    {"medium", {"medium",  3.00, 15.00, 50}},
    {"small",  {"small",   0.50,  2.00, 100}},
    {"local",  {"local",   0.00,  0.00, 15}},
};

const LocomoPreset& getLocomoPreset(const std::string& name) {
    std::string lower = name;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
    auto it = locomoPresets.find(lower);
    if (it != locomoPresets.end()) return it->second;
    return locomoPresets["medium"];
}

LocomoResult locomoEstimate(int64_t sumCode, int64_t sumComplexity,
                             const std::string& presetName,
                             double inputPriceOverride, double outputPriceOverride,
                             double tpsOverride, double reviewMinPerLine,
                             bool inputPriceSet, bool outputPriceSet, bool tpsSet,
                             double cyclesOverride, bool cyclesSet) {
    auto preset = getLocomoPreset(presetName);

    double inputPrice = inputPriceSet ? inputPriceOverride : preset.inputPrice;
    double outputPrice = outputPriceSet ? outputPriceOverride : preset.outputPrice;
    double tps = (tpsSet && tpsOverride > 0) ? tpsOverride : preset.tps;

    double density = sumCode ? (double)sumComplexity / (double)sumCode : 0.0;
    double cFactor = 1.0 + std::sqrt(density) * 5.0;  /* complexityWeight=5 */
    double iFactor = 1.5 + std::sqrt(density) * 2.0;   /* baseIterations=1.5, iterationWeight=2 */

    if (cyclesSet && cyclesOverride > 0) iFactor = cyclesOverride;

    double outputTokens = (double)sumCode * 10.0 * iFactor;   /* tokensPerLine=10 */
    double inputTokens = (double)sumCode * 20.0 * cFactor * iFactor; /* baseInputPerLine=20 */

    double cost = (inputTokens / 1'000'000.0) * inputPrice + (outputTokens / 1'000'000.0) * outputPrice;
    double genSeconds = (tps > 0) ? outputTokens / tps : 0.0;
    double reviewHours = (double)sumCode * reviewMinPerLine / 60.0;

    return {inputTokens, outputTokens, cost, genSeconds, reviewHours, iFactor, preset.name};
}

std::string formatLocomo(const LocomoResult& r, const std::string& currency) {
    char buf[512];
    std::string out;
    snprintf(buf, sizeof(buf), "LOCOMO LLM Cost Estimate (%s)\n", r.preset.c_str());
    out += buf;
    snprintf(buf, sizeof(buf), "  Tokens Required (in/out) %.1fM / %.1fM\n",
             r.inputTokens / 1'000'000.0, r.outputTokens / 1'000'000.0);
    out += buf;
    snprintf(buf, sizeof(buf), "  Cost to Generate %s%s\n",
             currency.c_str(), formatIntWithCommas((int64_t)r.cost).c_str());
    out += buf;
    snprintf(buf, sizeof(buf), "  Estimated Cycles %.1f\n", r.iterationFactor);
    out += buf;

    if (r.generationSeconds > 86400)
        snprintf(buf, sizeof(buf), "  Generation Time (serial) %.1f days\n", r.generationSeconds / 86400.0);
    else if (r.generationSeconds > 3600)
        snprintf(buf, sizeof(buf), "  Generation Time (serial) %.1f hours\n", r.generationSeconds / 3600.0);
    else
        snprintf(buf, sizeof(buf), "  Generation Time (serial) %.1f minutes\n", r.generationSeconds / 60.0);
    out += buf;

    snprintf(buf, sizeof(buf), "  Human Review Time %.1f hours\n", r.reviewHours);
    out += buf;
    out += "  Disclaimer: rough ballpark for regenerating code using a LLM.\n";
    out += "  Does not account for context reuse, test generation, or heavy debugging.\n";
    return out;
}

/* ---- Size ---- */

std::string formatSize(int64_t sumBytes, const std::string& sizeUnit) {
    double size;
    std::string unit = sizeUnit;
    char buf[256];

    if (unit == "binary") {
        size = (double)sumBytes / 1048576.0;
    } else if (unit == "mixed") {
        size = (double)sumBytes / 1024000.0;
    } else if (unit == "xkcd-kb") {
        time_t t = time(nullptr);
        struct tm* tm_info = localtime(&t);
        int year = tm_info->tm_year + 1900;
        size = isLeapYear(year) ? (double)sumBytes / 1000000.0 : (double)sumBytes / 1048576.0;
    } else if (unit == "xkcd-kelly") {
        size = (double)sumBytes / (1012.0 * 1012.0);
    } else if (unit == "xkcd-imaginary") {
        snprintf(buf, sizeof(buf), "Processed %ld bytes, ¯\\_(ツ)_/¯ megabytes (XKCD-IMAGINARY)\n", sumBytes);
        return buf;
    } else if (unit == "xkcd-intel") {
        size = (double)sumBytes / (1023.937528 * 1023.937528);
    } else if (unit == "xkcd-drive") {
        time_t t = time(nullptr);
        struct tm* tm_info = localtime(&t);
        int year = tm_info->tm_year + 1900;
        int s = 908 - ((year - 2013) * 4);
        if (s < 1) s = 1;
        size = (double)sumBytes / (double)(s * s);
    } else if (unit == "xkcd-bakers") {
        size = (double)sumBytes / (1152.0 * 1152.0);
    } else {
        size = (double)sumBytes / 1000000.0;
        unit = "SI";
    }

    for (auto& c : unit) c = (char)std::toupper((unsigned char)c);
    snprintf(buf, sizeof(buf), "Processed %ld bytes, %.3f megabytes (%s)\n", sumBytes, size, unit.c_str());
    return buf;
}
