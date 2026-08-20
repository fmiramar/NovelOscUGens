// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>

namespace novelosc {

constexpr double kPi =
    3.141592653589793238462643383279502884;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kInvTwoPi = 1.0 / kTwoPi;
constexpr double kDenormalFloor = 1.0e-30;
constexpr double kOutputLimit = 16.0;
constexpr int kMaxExactFloatInteger = 16777216;

template <typename T>
inline T clampValue(T value, T low, T high)
{
    return std::min(std::max(value, low), high);
}

inline float finiteOr(float value, float fallback = 0.f)
{
    return std::isfinite(value) ? value : fallback;
}

inline double finiteOr(double value, double fallback = 0.0)
{
    return std::isfinite(value) ? value : fallback;
}

inline bool exactIntegerInRange(
    float value, int minimum, int maximum, int* result)
{
    if (!result || !std::isfinite(value)
        || value < static_cast<float>(minimum)
        || value > static_cast<float>(maximum))
        return false;
    const int converted = static_cast<int>(value);
    if (static_cast<float>(converted) != value)
        return false;
    *result = converted;
    return true;
}

inline int roundedClampedInteger(
    float value, int minimum, int maximum, int fallback)
{
    if (!std::isfinite(value))
        return clampValue(fallback, minimum, maximum);
    const double bounded = clampValue(
        static_cast<double>(value),
        static_cast<double>(minimum),
        static_cast<double>(maximum));
    return static_cast<int>(std::lround(bounded));
}

inline double flushDenormal(double value)
{
    return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

inline float safeOutput(double value)
{
    if (!std::isfinite(value))
        return 0.f;
    value = clampValue(value, -kOutputLimit, kOutputLimit);
    value = flushDenormal(value);
    return static_cast<float>(value);
}

inline double wrapCycles(double phase)
{
    if (!std::isfinite(phase))
        return 0.0;
    phase -= std::floor(phase);
    return phase >= 1.0 ? 0.0 : phase;
}

inline double smoothstep(double value)
{
    value = clampValue(value, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

inline double nyquistFade(
    double absoluteFrequency, double nyquist,
    double fadeStartFraction = 0.9)
{
    if (!(nyquist > 0.0)
        || absoluteFrequency >= nyquist)
        return 0.0;
    const double start =
        nyquist * clampValue(fadeStartFraction, 0.0, 0.999);
    if (absoluteFrequency <= start)
        return 1.0;
    return 1.0
        - smoothstep(
            (absoluteFrequency - start)
            / std::max(nyquist - start, 1.0e-20));
}

inline int floorMod(int value, int modulus)
{
    const int remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

inline double equalPowerA(double position)
{
    return std::cos(
        clampValue(position, 0.0, 1.0) * kPi * 0.5);
}

inline double equalPowerB(double position)
{
    return std::sin(
        clampValue(position, 0.0, 1.0) * kPi * 0.5);
}

inline double semitoneRatio(double semitones)
{
    return std::exp2(
        clampValue(semitones, -96.0, 96.0) / 12.0);
}

inline bool isPowerOfTwo(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

inline int rowMajorIndex(int row, int column, int width)
{
    return row * width + column;
}

inline std::complex<double> finiteGeometricSeries(
    std::complex<double> ratio, int count)
{
    if (count <= 0)
        return { 0.0, 0.0 };
    if (count == 1)
        return { 1.0, 0.0 };
    const std::complex<double> denominator =
        std::complex<double>(1.0, 0.0) - ratio;
    if (std::abs(denominator) < 1.0e-12)
        return { static_cast<double>(count), 0.0 };
    return (
        std::complex<double>(1.0, 0.0)
        - std::pow(ratio, count))
        / denominator;
}

inline double geometricMagnitudeSum(
    double magnitude, int count)
{
    if (count <= 0)
        return 0.0;
    if (std::abs(1.0 - magnitude) < 1.0e-10)
        return static_cast<double>(count);
    return (
        1.0 - std::pow(magnitude, count))
        / (1.0 - magnitude);
}

} // namespace novelosc
