/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MSM_NVIDIA_SMI_H
#define MSM_NVIDIA_SMI_H

#include <string>
#include <vector>

namespace Nvidia {
struct Sample {
    std::string uuid;
    std::string name;
    // Negative values mean unavailable, never zero usage.
    double utilization = -1;
    double memory_used = -1;  // MiB, as returned by nvidia-smi
    double memory_total = -1;
    double temperature = -1;  // Celsius
};

// Columns: uuid, utilization.gpu, memory.used, memory.total, temperature.gpu, name.
std::vector<Sample> parse_samples(const std::string& output);
}
#endif
