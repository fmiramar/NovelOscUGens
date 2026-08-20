// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <algorithm>
#include <cmath>

namespace novelosc {

inline void fftInPlace(
    double* real, double* imaginary, int size,
    bool inverse)
{
    for (int index = 1, reversed = 0; index < size;
         ++index) {
        int bit = size >> 1;
        while (reversed & bit) {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(real[index], real[reversed]);
            std::swap(
                imaginary[index], imaginary[reversed]);
        }
    }

    for (int length = 2; length <= size; length <<= 1) {
        const double angle =
            (inverse ? 1.0 : -1.0)
            * kTwoPi / static_cast<double>(length);
        const double rootReal = std::cos(angle);
        const double rootImaginary = std::sin(angle);
        for (int start = 0; start < size;
             start += length) {
            double factorReal = 1.0;
            double factorImaginary = 0.0;
            for (int offset = 0; offset < length / 2;
                 ++offset) {
                const int even = start + offset;
                const int odd = even + length / 2;
                const double oddReal =
                    real[odd] * factorReal
                    - imaginary[odd] * factorImaginary;
                const double oddImaginary =
                    real[odd] * factorImaginary
                    + imaginary[odd] * factorReal;
                const double evenReal = real[even];
                const double evenImaginary =
                    imaginary[even];
                real[even] = evenReal + oddReal;
                imaginary[even] =
                    evenImaginary + oddImaginary;
                real[odd] = evenReal - oddReal;
                imaginary[odd] =
                    evenImaginary - oddImaginary;

                const double nextReal =
                    factorReal * rootReal
                    - factorImaginary * rootImaginary;
                factorImaginary =
                    factorReal * rootImaginary
                    + factorImaginary * rootReal;
                factorReal = nextReal;
            }
        }
    }

    if (inverse) {
        const double scale = 1.0 / size;
        for (int index = 0; index < size; ++index) {
            real[index] *= scale;
            imaginary[index] *= scale;
        }
    }
}

inline void inverseWalsh(double* values, int size)
{
    for (int length = 1; length < size; length <<= 1) {
        for (int start = 0; start < size;
             start += length << 1) {
            for (int offset = 0; offset < length;
                 ++offset) {
                const double a = values[start + offset];
                const double b =
                    values[start + offset + length];
                values[start + offset] = a + b;
                values[start + offset + length] = a - b;
            }
        }
    }
    const double scale = 1.0 / std::sqrt(size);
    for (int index = 0; index < size; ++index)
        values[index] *= scale;
}

inline void inverseHaar(
    double* values, double* scratch, int size)
{
    constexpr double inverseRootTwo =
        0.70710678118654752440;
    for (int length = 1; length < size; length <<= 1) {
        const int outputLength = length << 1;
        std::fill(
            scratch, scratch + outputLength, 0.0);
        for (int index = 0; index < length; ++index) {
            const double approximation = values[index];
            const double detail = values[length + index];
            scratch[index * 2] =
                (approximation + detail) * inverseRootTwo;
            scratch[index * 2 + 1] =
                (approximation - detail) * inverseRootTwo;
        }
        std::copy_n(scratch, outputLength, values);
    }
}

inline void inverseDaubechies4(
    double* values, double* scratch, int size)
{
    const double rootThree = std::sqrt(3.0);
    const double denominator = 4.0 * std::sqrt(2.0);
    const double h[4] {
        (1.0 + rootThree) / denominator,
        (3.0 + rootThree) / denominator,
        (3.0 - rootThree) / denominator,
        (1.0 - rootThree) / denominator
    };
    const double g[4] { h[3], -h[2], h[1], -h[0] };

    for (int length = 1; length < size; length <<= 1) {
        const int outputLength = length << 1;
        std::fill(
            scratch, scratch + outputLength, 0.0);
        for (int index = 0; index < length; ++index) {
            const double approximation = values[index];
            const double detail = values[length + index];
            for (int tap = 0; tap < 4; ++tap) {
                const int outputIndex =
                    (2 * index + tap) % outputLength;
                scratch[outputIndex] +=
                    h[tap] * approximation
                    + g[tap] * detail;
            }
        }
        std::copy_n(scratch, outputLength, values);
    }
}

} // namespace novelosc
