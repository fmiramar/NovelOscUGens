// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <algorithm>
#include <cmath>

namespace novelosc {

struct SpectralFramePair {
    int first = 0;
    int second = 0;
    double fraction = 0.0;
};

inline SpectralFramePair spectralFramePair(
    double frame, int frameCount)
{
    if (frameCount <= 1)
        return {};
    const double wrapped =
        wrapCycles(frame / static_cast<double>(frameCount))
        * frameCount;
    const int first = clampValue(
        static_cast<int>(std::floor(wrapped)),
        0, frameCount - 1);
    return {
        first,
        (first + 1) % frameCount,
        wrapped - static_cast<double>(first)
    };
}

inline std::size_t spectralFrameOffset(
    int frame, int partial, int partialCount)
{
    return (
        static_cast<std::size_t>(frame) * partialCount
        + partial)
        * 2;
}

inline double spectralFrameValue(
    const float* data, int partialCount,
    const SpectralFramePair& pair, int partial,
    int field)
{
    const double first = data[
        spectralFrameOffset(
            pair.first, partial, partialCount)
        + field];
    const double second = data[
        spectralFrameOffset(
            pair.second, partial, partialCount)
        + field];
    return first + (second - first) * pair.fraction;
}

inline double stretchedRatio(double ratio, double stretch)
{
    if (!std::isfinite(ratio) || !(ratio > 0.0)
        || !std::isfinite(stretch))
        return 0.0;
    return std::exp2(
        clampValue(std::log2(ratio) * stretch,
                   -24.0, 24.0));
}

inline double spectralTiltGain(double ratio, double dbPerOctave)
{
    if (!(ratio > 0.0) || !std::isfinite(dbPerOctave))
        return 0.0;
    const double decibels =
        clampValue(
            dbPerOctave * std::log2(ratio),
            -120.0, 120.0);
    return std::pow(10.0, decibels / 20.0);
}

inline double partialActivation(
    double activePartials, int partial)
{
    return clampValue(
        activePartials - static_cast<double>(partial),
        0.0, 1.0);
}

} // namespace novelosc
