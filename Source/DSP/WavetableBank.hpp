// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Interpolation.hpp"
#include "MathUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace novelosc {

struct WavetableBankView {
    const float* data = nullptr;
    int tablesX = 1;
    int tablesY = 1;
    int tablesZ = 1;
    int tableSize = 0;
    int mipLevels = 1;
    int interpolation = 2;
};

inline const float* wavetablePointer(
    const WavetableBankView& view, int mip,
    int z, int y, int x)
{
    const std::size_t tableIndex =
        (((static_cast<std::size_t>(mip) * view.tablesZ + z)
              * view.tablesY + y)
             * view.tablesX + x);
    return view.data + tableIndex * view.tableSize;
}

inline double readWavetableCorner(
    const WavetableBankView& view, int mip,
    int z, int y, int x, double phase)
{
    return readTable(
        wavetablePointer(view, mip, z, y, x),
        view.tableSize, phase, view.interpolation);
}

inline double coordinatePosition(
    double coordinate, int count)
{
    return clampValue(finiteOr(coordinate, 0.0), 0.0, 1.0)
        * static_cast<double>(std::max(count - 1, 0));
}

inline double readWavetable2D(
    const WavetableBankView& view, int mip,
    double x, double y, double phase)
{
    const double px = coordinatePosition(x, view.tablesX);
    const double py = coordinatePosition(y, view.tablesY);
    const int x0 = static_cast<int>(std::floor(px));
    const int y0 = static_cast<int>(std::floor(py));
    const int x1 = std::min(x0 + 1, view.tablesX - 1);
    const int y1 = std::min(y0 + 1, view.tablesY - 1);
    const double fx = px - x0;
    const double fy = py - y0;
    const double a = readWavetableCorner(
        view, mip, 0, y0, x0, phase);
    const double b = readWavetableCorner(
        view, mip, 0, y0, x1, phase);
    const double c = readWavetableCorner(
        view, mip, 0, y1, x0, phase);
    const double d = readWavetableCorner(
        view, mip, 0, y1, x1, phase);
    return (a + (b - a) * fx)
        + ((c + (d - c) * fx)
            - (a + (b - a) * fx))
            * fy;
}

inline double readWavetable3D(
    const WavetableBankView& view, int mip,
    double x, double y, double z, double phase)
{
    const double px = coordinatePosition(x, view.tablesX);
    const double py = coordinatePosition(y, view.tablesY);
    const double pz = coordinatePosition(z, view.tablesZ);
    const int x0 = static_cast<int>(std::floor(px));
    const int y0 = static_cast<int>(std::floor(py));
    const int z0 = static_cast<int>(std::floor(pz));
    const int x1 = std::min(x0 + 1, view.tablesX - 1);
    const int y1 = std::min(y0 + 1, view.tablesY - 1);
    const int z1 = std::min(z0 + 1, view.tablesZ - 1);
    const double fx = px - x0;
    const double fy = py - y0;
    const double fz = pz - z0;
    const auto plane = [&](int zi) {
        const double a = readWavetableCorner(
            view, mip, zi, y0, x0, phase);
        const double b = readWavetableCorner(
            view, mip, zi, y0, x1, phase);
        const double c = readWavetableCorner(
            view, mip, zi, y1, x0, phase);
        const double d = readWavetableCorner(
            view, mip, zi, y1, x1, phase);
        const double top = a + (b - a) * fx;
        const double bottom = c + (d - c) * fx;
        return top + (bottom - top) * fy;
    };
    const double first = plane(z0);
    const double second = plane(z1);
    return first + (second - first) * fz;
}

inline double wavetableMipPosition(
    double frequency, double sampleRate,
    int tableSize, int mipLevels)
{
    if (mipLevels <= 1)
        return 0.0;
    const double ratio =
        std::abs(frequency) * tableSize
        / std::max(sampleRate, 1.0);
    const double position =
        ratio > 1.0 ? std::log2(ratio) : 0.0;
    return clampValue(
        position, 0.0,
        static_cast<double>(mipLevels - 1));
}

} // namespace novelosc
