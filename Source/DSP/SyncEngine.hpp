// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <algorithm>

namespace novelosc {

inline bool risingEdge(double current, double previous)
{
    return current > 0.0 && previous <= 0.0;
}

inline double shortestCycleDifference(
    double target, double source)
{
    double difference =
        wrapCycles(target) - wrapCycles(source);
    if (difference > 0.5)
        difference -= 1.0;
    else if (difference < -0.5)
        difference += 1.0;
    return difference;
}

struct CausalBlep {
    static constexpr int kSlots = 8;
    double amplitudes[kSlots] {};
    int positions[kSlots] {};
    int lengths[kSlots] {};

    void add(double amplitude, int length)
    {
        int slot = 0;
        for (int index = 0; index < kSlots; ++index) {
            if (positions[index] >= lengths[index]) {
                slot = index;
                break;
            }
            if (positions[index] > positions[slot])
                slot = index;
        }
        amplitudes[slot] = finiteOr(amplitude, 0.0);
        positions[slot] = 0;
        lengths[slot] = std::max(length, 2);
    }

    double process()
    {
        double correction = 0.0;
        for (int index = 0; index < kSlots; ++index) {
            if (positions[index] >= lengths[index])
                continue;
            const double position =
                static_cast<double>(positions[index])
                / static_cast<double>(lengths[index] - 1);
            correction += amplitudes[index]
                * (1.0 - smoothstep(position));
            ++positions[index];
        }
        return correction;
    }
};

} // namespace novelosc
