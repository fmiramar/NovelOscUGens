// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/BufferValidation.hpp"
#include "DSP/ShiftRegister.hpp"
#include "DSP/SpectralFrameBank.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

struct SpectralFrameOsc : public Unit {
    double* phases = nullptr;
    double* activeGains = nullptr;
    double* stateSines = nullptr;
    double* stateCosines = nullptr;
    double* rotationSines = nullptr;
    double* rotationCosines = nullptr;
    double* currentWeights = nullptr;
    double* targetWeights = nullptr;
    int bufferNumber = -1;
    int frames = 0;
    int partials = 0;
    int phaseMode = 0;
    int expectedSamples = 0;
    SndBuf* buffer = nullptr;
    float* data = nullptr;
    float previous[6] {};
    int processingLimit = 0;
    bool optimizedControlRate = false;
    bool failed = false;
};

double interpolatedAmplitude(
    const SpectralFrameOsc* unit,
    const novelosc::SpectralFramePair& pair,
    int partial)
{
    return novelosc::spectralFrameValue(
        unit->data, unit->partials, pair, partial, 1);
}

void SpectralFrameOsc_nextAudioRate(
    SpectralFrameOsc* unit, int inNumSamples)
{
    if (unit->failed
        || !novelosc::bufferIdentityMatches(
            unit, unit->bufferNumber, unit->buffer,
            unit->data, unit->expectedSamples)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    const double sampleRate = unit->mRate->mSampleRate;
    const double nyquist = sampleRate * 0.5;
    const double gainCoefficient =
        1.0 - std::exp(
            -1.0 / std::max(0.01 * sampleRate, 1.0));
    float* output = OUT(0);
    SndBuf* buffer = unit->buffer;
    LOCK_SNDBUF_SHARED(buffer);
    if (!buffer->data || buffer->data != unit->data) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    for (int sample = 0; sample < inNumSamples; ++sample) {
        const double frequency = novelosc::rampedInput(
            unit, 1, sample, inNumSamples,
            unit->previous[0], 110.f);
        const double frame = novelosc::rampedInput(
            unit, 2, sample, inNumSamples,
            unit->previous[1], 0.f);
        const double stretch = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 3, sample, inNumSamples,
                unit->previous[2], 1.f)),
            -4.0, 4.0);
        const double tilt = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 4, sample, inNumSamples,
                unit->previous[3], 0.f)),
            -48.0, 48.0);
        const double blur = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 5, sample, inNumSamples,
                unit->previous[4], 0.f)),
            0.0, 1.0);
        const double active = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 6, sample, inNumSamples,
                unit->previous[5], 64.f)),
            0.0, static_cast<double>(unit->partials));
        const novelosc::SpectralFramePair pair =
            novelosc::spectralFramePair(frame, unit->frames);

        double sum = 0.0;
        double energy = 0.0;
        for (int partial = 0; partial < unit->partials; ++partial) {
            const double target =
                novelosc::partialActivation(active, partial);
            unit->activeGains[partial] += gainCoefficient
                * (target - unit->activeGains[partial]);

            const double storedRatio =
                novelosc::spectralFrameValue(
                    unit->data, unit->partials,
                    pair, partial, 0);
            const double ratio =
                novelosc::stretchedRatio(storedRatio, stretch);
            if (!(ratio > 0.0)) {
                unit->activeGains[partial] = 0.0;
                continue;
            }
            const double center =
                interpolatedAmplitude(unit, pair, partial);
            const double left = interpolatedAmplitude(
                unit, pair, std::max(partial - 1, 0));
            const double right = interpolatedAmplitude(
                unit, pair,
                std::min(partial + 1, unit->partials - 1));
            const double blurred =
                center + blur
                    * ((left + 2.0 * center + right) * 0.25
                       - center);
            const double partialFrequency =
                std::abs(frequency * ratio);
            const double weight =
                blurred
                * novelosc::spectralTiltGain(ratio, tilt)
                * novelosc::nyquistFade(
                    partialFrequency, nyquist, 0.9)
                * unit->activeGains[partial];
            sum += weight
                * std::sin(novelosc::kTwoPi
                           * unit->phases[partial]);
            energy += weight * weight;
            unit->phases[partial] = novelosc::wrapCycles(
                unit->phases[partial]
                + novelosc::clampValue(
                    frequency * ratio / sampleRate,
                    -8.0, 8.0));
        }
        output[sample] = energy > 1.0e-20
            ? novelosc::safeOutput(
                sum * std::sqrt(0.5 / energy))
            : 0.f;
    }

    const float defaults[6] {
        110.f, 0.f, 1.f, 0.f, 0.f, 64.f
    };
    for (int input = 0; input < 6; ++input)
        novelosc::finishRampedInput(
            unit, input + 1, unit->previous[input],
            defaults[input]);
}

void SpectralFrameOsc_nextControlRate(
    SpectralFrameOsc* unit, int inNumSamples)
{
    if (unit->failed
        || !novelosc::bufferIdentityMatches(
            unit, unit->bufferNumber, unit->buffer,
            unit->data, unit->expectedSamples)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    const double sampleRate = unit->mRate->mSampleRate;
    const double nyquist = sampleRate * 0.5;
    const double frequency = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(1), 110.f)),
        -sampleRate * 8.0, sampleRate * 8.0);
    const double frame =
        novelosc::finiteOr(IN0(2), 0.f);
    const double stretch = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(3), 1.f)),
        -4.0, 4.0);
    const double tilt = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(4), 0.f)),
        -48.0, 48.0);
    const double blur = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(5), 0.f)),
        0.0, 1.0);
    const double active = novelosc::clampValue(
        static_cast<double>(
            novelosc::finiteOr(IN0(6), 64.f)),
        0.0, static_cast<double>(unit->partials));
    const int newLimit = std::min(
        unit->partials,
        static_cast<int>(std::ceil(active)));
    const int workLimit = std::max(
        unit->processingLimit, newLimit);
    const novelosc::SpectralFramePair pair =
        novelosc::spectralFramePair(frame, unit->frames);
    const double inverseBlock =
        1.0 / std::max(inNumSamples, 1);
    SndBuf* buffer = unit->buffer;
    LOCK_SNDBUF_SHARED(buffer);
    if (!buffer->data || buffer->data != unit->data) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    for (int partial = 0; partial < unit->partials; ++partial) {
        if (partial < workLimit) {
            if (partial >= unit->processingLimit) {
                unit->stateSines[partial] = std::sin(
                    novelosc::kTwoPi * unit->phases[partial]);
                unit->stateCosines[partial] = std::cos(
                    novelosc::kTwoPi * unit->phases[partial]);
            }
            const double magnitude = std::hypot(
                unit->stateSines[partial],
                unit->stateCosines[partial]);
            if (std::isfinite(magnitude) && magnitude > 1.0e-20) {
                unit->stateSines[partial] /= magnitude;
                unit->stateCosines[partial] /= magnitude;
            } else {
                unit->stateSines[partial] = 0.0;
                unit->stateCosines[partial] = 1.0;
            }
        }

        const double storedRatio =
            novelosc::spectralFrameValue(
                unit->data, unit->partials,
                pair, partial, 0);
        const double ratio =
            novelosc::stretchedRatio(storedRatio, stretch);
        double weight = 0.0;
        double increment = 0.0;
        if (ratio > 0.0) {
            increment = novelosc::clampValue(
                frequency * ratio / sampleRate,
                -8.0, 8.0);
            if (partial < workLimit) {
                const double center =
                    interpolatedAmplitude(unit, pair, partial);
                const double left = interpolatedAmplitude(
                    unit, pair, std::max(partial - 1, 0));
                const double right = interpolatedAmplitude(
                    unit, pair,
                    std::min(partial + 1, unit->partials - 1));
                const double blurred =
                    center + blur
                        * ((left + 2.0 * center + right) * 0.25
                           - center);
                weight =
                    blurred
                    * novelosc::spectralTiltGain(ratio, tilt)
                    * novelosc::nyquistFade(
                        std::abs(frequency * ratio),
                        nyquist, 0.9)
                    * novelosc::partialActivation(active, partial);
            }
        }
        unit->phases[partial] = novelosc::wrapCycles(
            unit->phases[partial]
            + increment * inNumSamples);
        unit->targetWeights[partial] =
            novelosc::finiteOr(weight, 0.0);
        unit->activeGains[partial] =
            (unit->targetWeights[partial]
             - unit->currentWeights[partial])
            * inverseBlock;
        if (partial < workLimit) {
            const double angle =
                novelosc::kTwoPi * increment;
            unit->rotationSines[partial] = std::sin(angle);
            unit->rotationCosines[partial] = std::cos(angle);
        } else {
            unit->currentWeights[partial] = 0.0;
            unit->activeGains[partial] = 0.0;
        }
    }

    float* output = OUT(0);
    for (int sample = 0; sample < inNumSamples; ++sample) {
        double sum = 0.0;
        double energy = 0.0;
        for (int partial = 0; partial < workLimit; ++partial) {
            unit->currentWeights[partial] +=
                unit->activeGains[partial];
            const double weight =
                unit->currentWeights[partial];
            sum += weight * unit->stateSines[partial];
            energy += weight * weight;
            const double oldSine =
                unit->stateSines[partial];
            const double oldCosine =
                unit->stateCosines[partial];
            unit->stateSines[partial] =
                oldSine * unit->rotationCosines[partial]
                + oldCosine * unit->rotationSines[partial];
            unit->stateCosines[partial] =
                oldCosine * unit->rotationCosines[partial]
                - oldSine * unit->rotationSines[partial];
        }
        output[sample] = energy > 1.0e-20
            ? novelosc::safeOutput(
                sum * std::sqrt(0.5 / energy))
            : 0.f;
    }
    for (int partial = 0; partial < workLimit; ++partial)
        unit->currentWeights[partial] =
            unit->targetWeights[partial];
    unit->processingLimit = newLimit;
}

void SpectralFrameOsc_Ctor(SpectralFrameOsc* unit)
{
    SETCALC(SpectralFrameOsc_nextAudioRate);
    unit->phases = nullptr;
    unit->activeGains = nullptr;
    unit->stateSines = nullptr;
    unit->stateCosines = nullptr;
    unit->rotationSines = nullptr;
    unit->rotationCosines = nullptr;
    unit->currentWeights = nullptr;
    unit->targetWeights = nullptr;
    unit->bufferNumber = -1;
    unit->frames = 0;
    unit->partials = 0;
    unit->phaseMode = 0;
    unit->expectedSamples = 0;
    unit->buffer = nullptr;
    unit->data = nullptr;
    std::fill(unit->previous, unit->previous + 6, 0.f);
    unit->processingLimit = 0;
    unit->optimizedControlRate = false;
    unit->failed = false;
    int seed = 0;

    bool valid =
        novelosc::exactInitInt(
            IN0(0), 0, novelosc::kMaxExactFloatInteger - 1,
            &unit->bufferNumber)
        && novelosc::exactInitInt(IN0(7), 0, 2, &unit->phaseMode)
        && novelosc::exactInitInt(IN0(8), 1, 65536, &unit->frames)
        && novelosc::exactInitInt(IN0(9), 1, 1024, &unit->partials)
        && novelosc::exactInitInt(
            IN0(10), 0, novelosc::kMaxExactFloatInteger - 1,
            &seed);
    const int64_t expected =
        static_cast<int64_t>(unit->frames)
        * unit->partials * 2;
    valid = valid && expected > 0
        && expected <= std::numeric_limits<int>::max();
    if (valid)
        unit->expectedSamples = static_cast<int>(expected);
    if (valid)
        unit->buffer =
            novelosc::resolveBuffer(unit, unit->bufferNumber);
    valid = valid && unit->buffer && unit->buffer->data
        && unit->buffer->channels == 1
        && static_cast<int>(unit->buffer->samples)
            == unit->expectedSamples;
    if (!valid) {
        unit->failed = true;
        Print("SpectralFrameOsc: invalid buffer or frame metadata.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    unit->data = unit->buffer->data;
    unit->phases =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    unit->activeGains =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    unit->stateSines =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    unit->stateCosines =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    unit->rotationSines =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    unit->rotationCosines =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    unit->currentWeights =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    unit->targetWeights =
        novelosc::allocateRT<double>(unit->mWorld, unit->partials);
    if (!unit->phases || !unit->activeGains
        || !unit->stateSines || !unit->stateCosines
        || !unit->rotationSines || !unit->rotationCosines
        || !unit->currentWeights || !unit->targetWeights) {
        unit->failed = true;
        Print("SpectralFrameOsc: real-time allocation failed.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }

    novelosc::XorShift32 random;
    random.seed(static_cast<uint32_t>(seed));
    for (int partial = 0; partial < unit->partials; ++partial) {
        if (unit->phaseMode == 1)
            unit->phases[partial] =
                static_cast<double>(partial) / unit->partials;
        else if (unit->phaseMode == 2)
            unit->phases[partial] = random.uniform();
        unit->stateSines[partial] = std::sin(
            novelosc::kTwoPi * unit->phases[partial]);
        unit->stateCosines[partial] = std::cos(
            novelosc::kTwoPi * unit->phases[partial]);
        unit->activeGains[partial] =
            novelosc::partialActivation(
                novelosc::finiteOr(IN0(6), 64.f), partial);
    }
    unit->processingLimit = novelosc::clampValue(
        static_cast<int>(std::ceil(
            novelosc::finiteOr(IN0(6), 64.f))),
        0, unit->partials);
    unit->optimizedControlRate = true;
    for (int input = 1; input <= 6; ++input) {
        if (INRATE(input) == calc_FullRate)
            unit->optimizedControlRate = false;
    }
    if (unit->optimizedControlRate)
        SETCALC(SpectralFrameOsc_nextControlRate);
    const float defaults[6] {
        110.f, 0.f, 1.f, 0.f, 0.f, 64.f
    };
    for (int input = 0; input < 6; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input + 1), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

void SpectralFrameOsc_Dtor(SpectralFrameOsc* unit)
{
    novelosc::releaseRT(unit->mWorld, unit->phases);
    novelosc::releaseRT(unit->mWorld, unit->activeGains);
    novelosc::releaseRT(unit->mWorld, unit->stateSines);
    novelosc::releaseRT(unit->mWorld, unit->stateCosines);
    novelosc::releaseRT(unit->mWorld, unit->rotationSines);
    novelosc::releaseRT(unit->mWorld, unit->rotationCosines);
    novelosc::releaseRT(unit->mWorld, unit->currentWeights);
    novelosc::releaseRT(unit->mWorld, unit->targetWeights);
}

} // namespace

void registerSpectralFrameOsc()
{
    DefineDtorCantAliasUnit(SpectralFrameOsc);
}
