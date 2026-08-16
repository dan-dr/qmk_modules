// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "detectors.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace clean_slate {

void canonicalize(Config& config) {
    config.cadence_ms = std::clamp<uint16_t>(config.cadence_ms, 1, 20);
    config.gap_ms = std::clamp<uint16_t>(config.gap_ms, 20, 1000);
    config.motion_mode &= 3U;
    if (config.frontend == Frontend::raw) config.frontend_param = 1;
    if (config.frontend == Frontend::mean ||
        config.frontend == Frontend::median) {
        config.frontend_param = std::clamp<uint16_t>(config.frontend_param, 1, 17);
        config.frontend_param |= 1U;
        if (config.frontend_param == 1) config.frontend = Frontend::raw;
    }
    if (config.frontend == Frontend::slew_limited) {
        config.frontend_param = std::clamp<uint16_t>(config.frontend_param, 1, 200);
    }
    if (config.frontend == Frontend::exponential) {
        config.frontend_param = std::clamp<uint16_t>(config.frontend_param, 2, 1000);
    }
    if (config.motion_mode == 0) {
        config.motion_window_ms = 0;
        config.motion_force_floor = 0;
        config.motion_threshold = 0;
    } else {
        config.motion_window_ms = std::clamp<uint16_t>(config.motion_window_ms, 1, 500);
        config.motion_force_floor = std::clamp<uint16_t>(config.motion_force_floor, 1, 500);
        config.motion_threshold = std::clamp<uint16_t>(config.motion_threshold, 1, 1000);
    }
    auto& p = config.p;
    p[0] = std::clamp(p[0], 1, 10000);
    const int cadence = config.cadence_ms;
    p[1] = std::clamp(p[1], 1, 64 * cadence);
    p[1] = ((p[1] + cadence - 1) / cadence) * cadence;
    p[2] = std::clamp(p[2], 1, 1000);
    p[3] = std::clamp(p[3], 1, 1000);
    p[4] = std::clamp(p[4], 1, 1500);
    p[5] = std::clamp(p[5], -500, p[4] - 1);
    p[6] = std::clamp(p[6], 0, 1000);
    p[7] = std::clamp(p[7], 0, 1000);
    p[8] = std::clamp(p[8], 1, p[2]);
    p[9] = 0;
}

}  // namespace clean_slate

int main(int argc, char **argv) {
    if (argc != 18) return 2;
    clean_slate::Config config;
    config.family = clean_slate::Family::bidirectional_excursion_latch;
    config.frontend = static_cast<clean_slate::Frontend>(std::strtol(argv[1], nullptr, 10));
    config.frontend_param = static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10));
    config.cadence_ms = static_cast<uint16_t>(std::strtoul(argv[3], nullptr, 10));
    config.gap_ms = static_cast<uint16_t>(std::strtoul(argv[4], nullptr, 10));
    config.motion_mode = static_cast<uint8_t>(std::strtoul(argv[5], nullptr, 10));
    config.motion_window_ms = static_cast<uint16_t>(std::strtoul(argv[6], nullptr, 10));
    config.motion_force_floor = static_cast<uint16_t>(std::strtoul(argv[7], nullptr, 10));
    config.motion_threshold = static_cast<uint16_t>(std::strtoul(argv[8], nullptr, 10));
    for (size_t index = 0; index < 9; ++index) {
        config.p[index] = static_cast<int32_t>(std::strtol(argv[index + 9], nullptr, 10));
    }
    clean_slate::Series series;
    unsigned reading;
    unsigned t_ms;
    int motion_x;
    int motion_y;
    while (std::cin >> reading && std::cin.get() == ',' &&
           std::cin >> t_ms && std::cin.get() == ',' &&
           std::cin >> motion_x && std::cin.get() == ',' &&
           std::cin >> motion_y) {
        clean_slate::Sample sample;
        sample.value = static_cast<int16_t>(reading);
        sample.raw_ms = static_cast<uint32_t>(t_ms);
        sample.mono_ms = t_ms;
        sample.motion_x = static_cast<int16_t>(motion_x);
        sample.motion_y = static_cast<int16_t>(motion_y);
        series.samples.push_back(sample);
    }
    for (uint8_t value : clean_slate::replay(config, series)) {
        std::cout << static_cast<unsigned>(value) << '\n';
    }
    return 0;
}
