/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nvidia-smi.h"
#include <cstdlib>
#include <iostream>

#define CHECK(value) do { if (!(value)) { std::cerr << "Failed line " << __LINE__ << '\n'; std::exit(1); } } while (0)
int main()
{
    CHECK(Nvidia::parse_samples("").empty());
    CHECK(Nvidia::parse_samples("No devices were found\n").empty());
    CHECK(Nvidia::parse_samples("NVIDIA-SMI has failed\n").empty());
    CHECK(Nvidia::parse_samples("GPU-a, 10, 20\n").empty());
    auto samples = Nvidia::parse_samples(
        "GPU-a, 37, 4096, 8192, 61, NVIDIA First\r\n"
        "GPU-b, N/A, [Not Supported], 16384, N/A, NVIDIA Second, Special\n");
    CHECK(samples.size() == 2);
    CHECK(samples[0].uuid == "GPU-a" && samples[1].uuid == "GPU-b");
    CHECK(samples[0].utilization == 37 && samples[0].memory_used == 4096);
    CHECK(samples[0].memory_total == 8192 && samples[0].temperature == 61);
    CHECK(samples[1].utilization == -1 && samples[1].temperature == -1);
    CHECK(samples[1].memory_used == -1 && samples[1].memory_total == 16384);
    CHECK(samples[1].name == "NVIDIA Second, Special");
    samples = Nvidia::parse_samples("GPU-a, 0, 0, 8192, 0, Idle\nGPU-a, 99, 1, 2, 3, Duplicate\n");
    CHECK(samples.size() == 1 && samples[0].utilization == 0 && samples[0].memory_used == 0);
    for (auto bad : {"-1", "nan", "inf", "101", "1%", "", "1e9999"}) {
        samples = Nvidia::parse_samples(std::string("GPU-a, ") + bad + ", 9, 8, -1, Bad\n");
        CHECK(samples.size() == 1 && samples[0].utilization == -1);
        CHECK(samples[0].memory_used == -1 && samples[0].memory_total == -1);
        CHECK(samples[0].temperature == -1);
    }
    samples = Nvidia::parse_samples("GPU-a, 1, 0, 0, 12, Zero\n");
    CHECK(samples[0].memory_total == -1);
    std::cout << "NVIDIA parser tests passed\n";
}
