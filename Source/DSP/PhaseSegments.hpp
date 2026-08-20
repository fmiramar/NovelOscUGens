// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <algorithm>
#include <cmath>

namespace novelosc {

constexpr int kMaximumPhaseBreakpoints = 8;

inline void sanitizePhaseBreakpoints(
    const double* sourceX, const double* sourceY, int count,
    double* x, double* y)
{
    constexpr double spacing = 1.0e-4;
    double sortedX[kMaximumPhaseBreakpoints] {};
    double sortedY[kMaximumPhaseBreakpoints] {};
    count = clampValue(count, 0, kMaximumPhaseBreakpoints);
    for (int index = 0; index < count; ++index) {
        const double candidateX = clampValue(
            finiteOr(sourceX[index], 0.5), spacing,
            1.0 - spacing);
        const double candidateY = clampValue(
            finiteOr(sourceY[index], candidateX), 0.0, 1.0);
        int insertion = index;
        while (insertion > 0
               && candidateX < sortedX[insertion - 1]) {
            sortedX[insertion] = sortedX[insertion - 1];
            sortedY[insertion] = sortedY[insertion - 1];
            --insertion;
        }
        sortedX[insertion] = candidateX;
        sortedY[insertion] = candidateY;
    }

    double previous = 0.0;
    for (int index = 0; index < count; ++index) {
        const double low = previous + spacing;
        const double high =
            1.0 - spacing * static_cast<double>(count - index);
        x[index] = clampValue(sortedX[index], low, high);
        y[index] = sortedY[index];
        previous = x[index];
    }
}

inline double mapPhaseSegments(
    double phase, const double* x, const double* y,
    int count)
{
    phase = wrapCycles(phase);
    double previousX = 0.0;
    double previousY = 0.0;
    for (int index = 0; index <= count; ++index) {
        const double nextX = index < count ? x[index] : 1.0;
        const double nextY = index < count ? y[index] : 1.0;
        if (phase < nextX || index == count) {
            const double fraction =
                (phase - previousX)
                / std::max(nextX - previousX, 1.0e-12);
            return previousY
                + (nextY - previousY) * fraction;
        }
        previousX = nextX;
        previousY = nextY;
    }
    return 0.0;
}

inline double maximumPhaseSegmentSlope(
    const double* x, const double* y, int count)
{
    double maximum = 0.0;
    double previousX = 0.0;
    double previousY = 0.0;
    for (int index = 0; index <= count; ++index) {
        const double nextX = index < count ? x[index] : 1.0;
        const double nextY = index < count ? y[index] : 1.0;
        maximum = std::max(
            maximum,
            std::abs(nextY - previousY)
                / std::max(nextX - previousX, 1.0e-12));
        previousX = nextX;
        previousY = nextY;
    }
    return maximum;
}

inline double bendUnitInterval(double value, double curve)
{
    value = clampValue(value, 0.0, 1.0);
    curve = clampValue(curve, -1.0, 1.0);
    return value + curve * value * (1.0 - value);
}

} // namespace novelosc
