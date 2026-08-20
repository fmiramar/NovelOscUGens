// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <algorithm>
#include <cmath>

namespace novelosc {

inline double polyBlep(double phase, double increment)
{
    const double width = std::abs(increment);
    if (!(width > 0.0) || width >= 1.0)
        return 0.0;
    phase = wrapCycles(phase);
    if (phase < width) {
        const double x = phase / width;
        return x + x - x * x - 1.0;
    }
    if (phase > 1.0 - width) {
        const double x = (phase - 1.0) / width;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

inline double directedSaw(double phase, double increment)
{
    if (increment >= 0.0)
        return 2.0 * wrapCycles(phase) - 1.0
            - polyBlep(phase, increment);
    const double reversed = wrapCycles(1.0 - phase);
    return 2.0 * reversed - 1.0
        - polyBlep(reversed, -increment);
}

inline double directedPulse(
    double phase, double increment, double width = 0.5)
{
    if (increment < 0.0)
        phase = wrapCycles(1.0 - phase);
    else
        phase = wrapCycles(phase);
    const double delta = std::abs(increment);
    width = clampValue(width, 0.01, 0.99);
    double value = phase < width ? 1.0 : -1.0;
    value += polyBlep(phase, delta);
    value -= polyBlep(wrapCycles(phase - width), delta);
    return value;
}

inline double polyBlamp(double phase, double increment)
{
    const double width = std::abs(increment);
    if (!(width > 0.0) || width >= 1.0)
        return 0.0;
    phase = wrapCycles(phase);
    if (phase < width) {
        const double x = phase / width - 1.0;
        return -(x * x * x) / 3.0;
    }
    if (phase > 1.0 - width) {
        const double x = (phase - 1.0) / width + 1.0;
        return (x * x * x) / 3.0;
    }
    return 0.0;
}

inline double asymmetricTriangle(
    double phase, double symmetry, double increment)
{
    phase = wrapCycles(phase);
    symmetry = clampValue(symmetry, 0.02, 0.98);
    const double risingSlope = 2.0 / symmetry;
    const double fallingSlope = -2.0 / (1.0 - symmetry);
    double value = phase < symmetry
        ? -1.0 + risingSlope * phase
        : 1.0 + fallingSlope * (phase - symmetry);

    const double cornerJump = fallingSlope - risingSlope;
    const double wrapJump = risingSlope - fallingSlope;
    value += cornerJump * std::abs(increment)
        * polyBlamp(wrapCycles(phase - symmetry), increment);
    value += wrapJump * std::abs(increment)
        * polyBlamp(phase, increment);
    return value;
}

struct BiquadLowpass {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double z1 = 0.0;
    double z2 = 0.0;

    void configure(double normalizedFrequency, double q)
    {
        normalizedFrequency = clampValue(
            normalizedFrequency, 1.0e-5, 0.49);
        q = std::max(q, 1.0e-4);
        const double omega =
            kTwoPi * normalizedFrequency;
        const double cosine = std::cos(omega);
        const double sine = std::sin(omega);
        const double alpha = sine / (2.0 * q);
        const double denominator = 1.0 + alpha;
        b0 = (1.0 - cosine) * 0.5 / denominator;
        b1 = (1.0 - cosine) / denominator;
        b2 = b0;
        a1 = -2.0 * cosine / denominator;
        a2 = (1.0 - alpha) / denominator;
    }

    double process(double input)
    {
        const double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        if (std::abs(z1) < kDenormalFloor)
            z1 = 0.0;
        if (std::abs(z2) < kDenormalFloor)
            z2 = 0.0;
        return output;
    }
};

struct OversamplingDecimator {
    int factor = 1;
    BiquadLowpass first;
    BiquadLowpass second;

    void configure(int newFactor)
    {
        factor = newFactor >= 8
            ? 8
            : (newFactor >= 4 ? 4 : (newFactor >= 2 ? 2 : 1));
        if (factor > 1) {
            const double cutoff =
                0.225 / static_cast<double>(factor);
            first.configure(cutoff, 0.541196100146197);
            second.configure(cutoff, 1.306562964876377);
        }
    }

    double process(double input)
    {
        if (factor == 1)
            return input;
        return second.process(first.process(input));
    }
};

} // namespace novelosc
