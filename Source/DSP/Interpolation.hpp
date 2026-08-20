// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <cmath>

namespace novelosc {

inline float readNearest(
    const float* table, int size, double phase)
{
    const double position = wrapCycles(phase) * size;
    const int index = floorMod(
        static_cast<int>(std::floor(position + 0.5)), size);
    return table[index];
}

inline float readLinear(
    const float* table, int size, double phase)
{
    const double position = wrapCycles(phase) * size;
    const int base = static_cast<int>(std::floor(position));
    const double fraction = position - base;
    const int a = floorMod(base, size);
    const int b = floorMod(base + 1, size);
    return static_cast<float>(
        table[a] + (table[b] - table[a]) * fraction);
}

inline float readCubic(
    const float* table, int size, double phase)
{
    const double position = wrapCycles(phase) * size;
    const int base = static_cast<int>(std::floor(position));
    const double t = position - base;
    const double y0 = table[floorMod(base - 1, size)];
    const double y1 = table[floorMod(base, size)];
    const double y2 = table[floorMod(base + 1, size)];
    const double y3 = table[floorMod(base + 2, size)];
    const double a0 =
        -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
    const double a1 =
        y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double a2 = -0.5 * y0 + 0.5 * y2;
    return static_cast<float>(
        ((a0 * t + a1) * t + a2) * t + y1);
}

inline float readTable(
    const float* table, int size, double phase,
    int interpolation)
{
    if (interpolation <= 0)
        return readNearest(table, size, phase);
    if (interpolation == 1)
        return readLinear(table, size, phase);
    return readCubic(table, size, phase);
}

} // namespace novelosc
