/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nvidia-smi.h"

#include <cmath>
#include <locale>
#include <set>
#include <sstream>

namespace {
std::string trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos)
        return {};
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}

double number(const std::string& text, double maximum)
{
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    double result;
    if (!(input >> result) || !std::isfinite(result) || result < 0 || result > maximum)
        return -1;
    input >> std::ws;
    return input.eof() ? result : -1;
}
}

std::vector<Nvidia::Sample> Nvidia::parse_samples(const std::string& output)
{
    std::vector<Sample> samples;
    std::set<std::string> seen;
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        std::string fields[6];
        size_t start = 0;
        bool valid = true;
        for (unsigned i = 0; i < 5; ++i) {
            const auto comma = line.find(',', start);
            if (comma == std::string::npos) {
                valid = false;
                break;
            }
            fields[i] = trim(line.substr(start, comma - start));
            start = comma + 1;
        }
        if (!valid)
            continue;
        fields[5] = trim(line.substr(start)); // Device names may contain commas.
        if (fields[0].compare(0, 4, "GPU-") != 0 || fields[0].size() <= 4 ||
            !seen.insert(fields[0]).second)
            continue;
        Sample sample;
        sample.uuid = fields[0];
        sample.name = fields[5].empty() ? sample.uuid : fields[5];
        sample.utilization = number(fields[1], 100);
        sample.memory_used = number(fields[2], 1e12);
        sample.memory_total = number(fields[3], 1e12);
        sample.temperature = number(fields[4], 1000);
        if (sample.memory_total == 0 ||
            (sample.memory_total >= 0 && sample.memory_used > sample.memory_total)) {
            sample.memory_used = sample.memory_total = -1;
        }
        samples.push_back(sample);
    }
    return samples;
}
