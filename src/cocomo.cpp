#include "cocomo.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <algorithm>

std::unordered_map<std::string, std::vector<double>> cocomoProjectTypes = {
    {"organic",       {2.4, 1.05, 2.5, 0.38}},
    {"semi-detached", {3.0, 1.12, 2.5, 0.35}},
    {"embedded",      {3.6, 1.20, 2.5, 0.32}},
};

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
            "Total Physical Source Lines of Code (SLOC)                     = %ld\n"
            "Development Effort Estimate, Person-Years (Person-Months)      = %.2f (%.2f)\n"
            " (Basic COCOMO model, Person-Months = %.2f*(KSLOC**%.2f)*%.2f)\n"
            "Schedule Estimate, Years (Months)                              = %.2f (%.2f)\n"
            " (Basic COCOMO model, Months = %.2f*(person-months**%.2f))\n"
            "Estimated Average Number of Developers (Effort/Schedule)       = %.2f\n"
            "Total Estimated Cost to Develop                                = %s%.0f\n"
            " (average salary = %s%ld/year, overhead = %.2f)\n",
            sumCode,
            effort / 12, effort,
            p[0], p[1], eaf,
            schedMonths / 12, schedMonths,
            p[2], p[3],
            people,
            currency.c_str(), cost,
            currency.c_str(), averageWage, overhead);
    } else {
        snprintf(buf, sizeof(buf),
            "Estimated Cost to Develop (%s) %s%.0f\n"
            "Estimated Schedule Effort (%s) %.2f months\n"
            "Estimated People Required (%s) %.2f\n",
            projectType.c_str(), currency.c_str(), cost,
            projectType.c_str(), schedMonths,
            projectType.c_str(), people);
    }
    out = buf;
    return out;
}

std::string formatSize(int64_t sumBytes, const std::string& sizeUnit) {
    double size;
    std::string unit = sizeUnit;
    std::string prefix;
    char buf[256];

    if (unit == "binary") {
        size = (double)sumBytes / 1048576.0;
    } else if (unit == "mixed") {
        size = (double)sumBytes / 1024000.0;
    } else if (unit == "xkcd-kb") {
        /* 1000 bytes during leap years, 1024 otherwise */
        time_t t = time(nullptr);
        struct tm* tm_info = localtime(&t);
        int year = tm_info->tm_year + 1900;
        if (isLeapYear(year)) {
            size = (double)sumBytes / 1000000.0;
        } else {
            size = (double)sumBytes / 1048576.0;
        }
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
        /* Default: SI */
        size = (double)sumBytes / 1000000.0;
        unit = "SI";
    }

    for (auto& c : unit) c = (char)std::toupper((unsigned char)c);
    snprintf(buf, sizeof(buf), "Processed %ld bytes, %.3f megabytes (%s)\n", sumBytes, size, unit.c_str());
    return buf;
}
