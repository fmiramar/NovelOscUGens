// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Bandlimit.hpp"
#include "MathUtils.hpp"

#include <algorithm>
#include <cmath>

namespace novelosc {

inline double bandlimitedTriangle(
    double phase, double increment)
{
    const double absoluteIncrement = std::abs(increment);
    if (!(absoluteIncrement > 0.0))
        return 4.0 * std::abs(wrapCycles(phase) - 0.5)
            - 1.0;
    const int maximumHarmonic = std::min(
        31,
        static_cast<int>(
            std::floor(0.5 / absoluteIncrement)));
    if (maximumHarmonic < 1)
        return 0.0;
    double sum = 0.0;
    for (int harmonic = 1; harmonic <= maximumHarmonic;
         harmonic += 2) {
        const double sign =
            ((harmonic - 1) / 2) & 1 ? -1.0 : 1.0;
        const double frequency =
            absoluteIncrement * harmonic;
        const double fade = nyquistFade(
            frequency, 0.5, 0.85);
        sum += sign * fade
            * std::sin(kTwoPi * harmonic * phase)
            / static_cast<double>(harmonic * harmonic);
    }
    return sum * (8.0 / (kPi * kPi));
}

inline double oscillatorShape(
    int waveform, double phase, double increment)
{
    phase = wrapCycles(phase);
    switch (waveform) {
    case 1:
        return bandlimitedTriangle(phase, increment);
    case 2:
        return directedSaw(phase, increment);
    case 3:
        return directedPulse(phase, increment);
    default:
        return std::sin(kTwoPi * phase);
    }
}

} // namespace novelosc
