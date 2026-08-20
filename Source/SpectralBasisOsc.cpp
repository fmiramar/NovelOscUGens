// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Interpolation.hpp"
#include "DSP/PhaseCore.hpp"
#include "DSP/SpectralTransforms.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

struct SpectralBasisOsc : public Unit {
    novelosc::PhaseCore phase;
    float previousFrequency = 440.f;
    float previousPhase = 0.f;
    int tableSize = 0;
    int mipLevels = 1;
    int currentBank = 0;
    int pendingBank = 1;
    int generatedBasis = 0;
    double generatedCenter = 0.25;
    double generatedWidth = 0.15;
    double generatedTilt = 0.0;
    double generatedSkew = 0.0;
    double fade = 1.0;
    double fadeIncrement = 1.0;
    int64_t sampleClock = 0;
    int64_t lastGeneration = 0;
    bool transitioning = false;
    float* banks = nullptr;
    double* coefficients = nullptr;
    double* real = nullptr;
    double* imaginary = nullptr;
    double* scratch = nullptr;
    bool failed = false;
    bool errorReported = false;
};

double coefficientEnvelope(
    int index, int count, double center, double width,
    double tilt, double skew)
{
    const double position = count > 1
        ? static_cast<double>(index) / (count - 1)
        : 0.0;
    const double sideScale = position < center
        ? 1.0 + skew
        : 1.0 - skew;
    const double localWidth = std::max(
        width * sideScale, 1.0e-4);
    const double distance =
        (position - center) / localWidth;
    const double envelope =
        std::exp(-0.5 * distance * distance);
    const double tiltWeight =
        std::exp(
            novelosc::clampValue(tilt, -8.0, 8.0)
            * (position - 0.5));
    return envelope * tiltWeight;
}

float* bankPointer(
    SpectralBasisOsc* unit, int bank, int mip)
{
    return unit->banks
        + (static_cast<std::size_t>(bank)
               * unit->mipLevels
           + mip)
            * unit->tableSize;
}

void normalizeTable(double* values, int size)
{
    double mean = 0.0;
    for (int index = 0; index < size; ++index)
        mean += values[index];
    mean /= size;
    double peak = 0.0;
    for (int index = 0; index < size; ++index) {
        values[index] -= mean;
        peak = std::max(peak, std::abs(values[index]));
    }
    const double scale = peak > 1.0e-20 ? 1.0 / peak : 0.0;
    for (int index = 0; index < size; ++index)
        values[index] *= scale;
}

void generateBank(
    SpectralBasisOsc* unit, int bank, int basis,
    double center, double width, double tilt,
    double skew)
{
    const int size = unit->tableSize;
    basis = novelosc::clampValue(basis, 0, 3);
    center = novelosc::clampValue(center, 0.0, 1.0);
    width = novelosc::clampValue(width, 0.005, 1.0);
    skew = novelosc::clampValue(skew, -0.95, 0.95);
    std::fill(unit->coefficients,
              unit->coefficients + size, 0.0);
    std::fill(unit->real, unit->real + size, 0.0);
    std::fill(
        unit->imaginary, unit->imaginary + size, 0.0);

    if (basis == 0) {
        const int harmonics = size / 2 - 1;
        for (int harmonic = 1; harmonic <= harmonics;
             ++harmonic) {
            const double amplitude = coefficientEnvelope(
                harmonic - 1, harmonics, center, width,
                tilt, skew);
            unit->real[harmonic] = amplitude;
            unit->real[size - harmonic] = amplitude;
        }
        novelosc::fftInPlace(
            unit->real, unit->imaginary, size, true);
    } else {
        for (int index = 0; index < size; ++index) {
            unit->coefficients[index] =
                coefficientEnvelope(
                    index, size, center, width, tilt, skew);
        }
        if (basis == 1)
            novelosc::inverseWalsh(
                unit->coefficients, size);
        else if (basis == 2)
            novelosc::inverseHaar(
                unit->coefficients, unit->scratch, size);
        else
            novelosc::inverseDaubechies4(
                unit->coefficients, unit->scratch, size);
        std::copy_n(
            unit->coefficients, size, unit->real);
    }

    normalizeTable(unit->real, size);
    float* base = bankPointer(unit, bank, 0);
    for (int index = 0; index < size; ++index)
        base[index] =
            novelosc::safeOutput(unit->real[index]);

    for (int mip = 1; mip < unit->mipLevels; ++mip) {
        for (int index = 0; index < size; ++index) {
            unit->real[index] = base[index];
            unit->imaginary[index] = 0.0;
        }
        novelosc::fftInPlace(
            unit->real, unit->imaginary, size, false);
        const int cutoff = std::max(1, (size / 2) >> mip);
        for (int bin = cutoff + 1;
             bin < size - cutoff; ++bin) {
            unit->real[bin] = 0.0;
            unit->imaginary[bin] = 0.0;
        }
        novelosc::fftInPlace(
            unit->real, unit->imaginary, size, true);
        float* destination = bankPointer(unit, bank, mip);
        for (int index = 0; index < size; ++index)
            destination[index] =
                novelosc::safeOutput(unit->real[index]);
    }
}

double readBank(
    SpectralBasisOsc* unit, int bank, double phase,
    double mipPosition)
{
    const int mip0 = static_cast<int>(
        std::floor(mipPosition));
    const int mip1 = std::min(
        mip0 + 1, unit->mipLevels - 1);
    const double fraction = mipPosition - mip0;
    const double first = novelosc::readCubic(
        bankPointer(unit, bank, mip0),
        unit->tableSize, phase);
    const double second = novelosc::readCubic(
        bankPointer(unit, bank, mip1),
        unit->tableSize, phase);
    return first + (second - first) * fraction;
}

void maybeRegenerate(
    SpectralBasisOsc* unit, int inNumSamples)
{
    if (unit->transitioning)
        return;
    const int basis = novelosc::roundedClampedInteger(
        IN0(1), 0, 3, 0);
    const double center = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(2), 0.25f)),
        0.0, 1.0);
    const double width = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(3), 0.15f)),
        0.005, 1.0);
    const double tilt = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(4), 0.f)),
        -8.0, 8.0);
    const double skew = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(5), 0.f)),
        -0.95, 0.95);
    const double refresh = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(7), 30.f)),
        0.1, 200.0);
    const int64_t interval = static_cast<int64_t>(
        unit->mRate->mSampleRate / refresh);
    const bool changed =
        basis != unit->generatedBasis
        || std::abs(center - unit->generatedCenter) > 1.0e-4
        || std::abs(width - unit->generatedWidth) > 1.0e-4
        || std::abs(tilt - unit->generatedTilt) > 1.0e-4
        || std::abs(skew - unit->generatedSkew) > 1.0e-4;
    if (!changed
        || unit->sampleClock - unit->lastGeneration
            < interval)
        return;

    unit->pendingBank = 1 - unit->currentBank;
    generateBank(
        unit, unit->pendingBank, basis, center, width,
        tilt, skew);
    unit->generatedBasis = basis;
    unit->generatedCenter = center;
    unit->generatedWidth = width;
    unit->generatedTilt = tilt;
    unit->generatedSkew = skew;
    unit->lastGeneration = unit->sampleClock;
    unit->fade = 0.0;
    unit->transitioning = true;
}

void SpectralBasisOsc_next(
    SpectralBasisOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    maybeRegenerate(unit, inNumSamples);
    const double sampleRate = unit->mRate->mSampleRate;
    float* output = OUT(0);

    for (int sample = 0; sample < inNumSamples; ++sample) {
        const double frequency =
            novelosc::rampedInput(
                unit, 0, sample, inNumSamples,
                unit->previousFrequency, 440.f);
        const double phaseOffset =
            novelosc::rampedInput(
                unit, 6, sample, inNumSamples,
                unit->previousPhase)
            * novelosc::kInvTwoPi;
        double mipPosition = 0.0;
        if (unit->mipLevels > 1) {
            const double ratio =
                std::abs(frequency) * unit->tableSize
                / sampleRate;
            if (ratio > 1.0)
                mipPosition = std::log2(ratio);
            mipPosition = novelosc::clampValue(
                mipPosition, 0.0,
                static_cast<double>(unit->mipLevels - 1));
        }
        const double readPhase =
            unit->phase.read(phaseOffset);
        const double current = readBank(
            unit, unit->currentBank, readPhase,
            mipPosition);
        double value = current;
        if (unit->transitioning) {
            const double pending = readBank(
                unit, unit->pendingBank, readPhase,
                mipPosition);
            value = current
                + (pending - current) * unit->fade;
            unit->fade += unit->fadeIncrement;
            if (unit->fade >= 1.0) {
                unit->fade = 1.0;
                unit->currentBank = unit->pendingBank;
                unit->transitioning = false;
            }
        }
        output[sample] = novelosc::safeOutput(value);
        unit->phase.advance(
            novelosc::clampValue(
                frequency / sampleRate, -8.0, 8.0));
    }
    novelosc::finishRampedInput(
        unit, 0, unit->previousFrequency, 440.f);
    novelosc::finishRampedInput(
        unit, 6, unit->previousPhase, 0.f);
    unit->sampleClock += inNumSamples;
}

void SpectralBasisOsc_Ctor(SpectralBasisOsc* unit)
{
    SETCALC(SpectralBasisOsc_next);
    unit->phase = {};
    unit->previousFrequency = 440.f;
    unit->previousPhase = 0.f;
    unit->tableSize = 0;
    unit->mipLevels = 1;
    unit->currentBank = 0;
    unit->pendingBank = 1;
    unit->generatedBasis = 0;
    unit->generatedCenter = 0.25;
    unit->generatedWidth = 0.15;
    unit->generatedTilt = 0.0;
    unit->generatedSkew = 0.0;
    unit->fade = 1.0;
    unit->fadeIncrement = 1.0;
    unit->sampleClock = 0;
    unit->lastGeneration = 0;
    unit->transitioning = false;
    unit->banks = nullptr;
    unit->coefficients = nullptr;
    unit->real = nullptr;
    unit->imaginary = nullptr;
    unit->scratch = nullptr;
    unit->failed = false;
    unit->errorReported = false;
    if (!novelosc::exactInitInt(
            IN0(8), 128, 4096, &unit->tableSize)
        || !novelosc::isPowerOfTwo(unit->tableSize)) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported,
            "SpectralBasisOsc",
            "tableSize must be a power of two from 128 to 4096.");
        return;
    }
    int size = unit->tableSize;
    unit->mipLevels = 1;
    while (size > 64 && unit->mipLevels < 8) {
        ++unit->mipLevels;
        size >>= 1;
    }

    const std::size_t tableSamples =
        static_cast<std::size_t>(2)
        * unit->mipLevels * unit->tableSize;
    unit->banks = novelosc::allocateRT<float>(
        unit->mWorld, tableSamples);
    unit->coefficients = novelosc::allocateRT<double>(
        unit->mWorld, unit->tableSize);
    unit->real = novelosc::allocateRT<double>(
        unit->mWorld, unit->tableSize);
    unit->imaginary = novelosc::allocateRT<double>(
        unit->mWorld, unit->tableSize);
    unit->scratch = novelosc::allocateRT<double>(
        unit->mWorld, unit->tableSize);
    if (!unit->banks || !unit->coefficients || !unit->real
        || !unit->imaginary || !unit->scratch) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported,
            "SpectralBasisOsc",
            "real-time table allocation failed.");
        return;
    }

    unit->generatedBasis = novelosc::roundedClampedInteger(
        IN0(1), 0, 3, 0);
    unit->generatedCenter = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(2), 0.25f)),
        0.0, 1.0);
    unit->generatedWidth = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(3), 0.15f)),
        0.005, 1.0);
    unit->generatedTilt = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(4), 0.f)),
        -8.0, 8.0);
    unit->generatedSkew = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(5), 0.f)),
        -0.95, 0.95);
    generateBank(
        unit, 0, unit->generatedBasis,
        unit->generatedCenter, unit->generatedWidth,
        unit->generatedTilt, unit->generatedSkew);
    generateBank(
        unit, 1, unit->generatedBasis,
        unit->generatedCenter, unit->generatedWidth,
        unit->generatedTilt, unit->generatedSkew);
    unit->previousFrequency =
        novelosc::finiteOr(IN0(0), 440.f);
    unit->previousPhase =
        novelosc::finiteOr(IN0(6), 0.f);
    unit->fadeIncrement =
        1.0 / std::max(
            unit->mRate->mSampleRate * 0.02, 1.0);
    ClearUnitOutputs(unit, 1);
}

void SpectralBasisOsc_Dtor(SpectralBasisOsc* unit)
{
    novelosc::releaseRT(unit->mWorld, unit->banks);
    novelosc::releaseRT(
        unit->mWorld, unit->coefficients);
    novelosc::releaseRT(unit->mWorld, unit->real);
    novelosc::releaseRT(
        unit->mWorld, unit->imaginary);
    novelosc::releaseRT(unit->mWorld, unit->scratch);
}

} // namespace

void registerSpectralBasisOsc()
{
    DefineDtorCantAliasUnit(SpectralBasisOsc);
}
