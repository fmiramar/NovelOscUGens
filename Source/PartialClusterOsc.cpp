// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/MathUtils.hpp"
#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kMaximumVoices = 5;

struct PartialClusterOsc : public Unit {
    novelosc::PhaseCore phases[kMaximumVoices];
    float previous[9] {};
    double* ratios = nullptr;
    double* stateSines = nullptr;
    double* stateCosines = nullptr;
    double* rotationSines = nullptr;
    double* rotationCosines = nullptr;
    double* currentWeights = nullptr;
    double* targetWeights = nullptr;
    double previousPhaseSpread = 0.0;
    int processingLimit = 0;
    int voices = 1;
    int maxPartials = 1;
    bool audioRateMode = false;
    bool failed = false;
    bool errorReported = false;
};

int voicePartialIndex(
    const PartialClusterOsc* unit, int voice, int partial)
{
    return voice * unit->maxPartials + partial;
}

void PartialClusterOsc_nextAudioRate(
    PartialClusterOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    const double nyquist = sampleRate * 0.5;
    float* leftOutput = OUT(0);
    float* rightOutput = OUT(1);

    for (int sample = 0; sample < inNumSamples; ++sample) {
        const int inputIndices[9] {
            0, 1, 2, 3, 4, 5, 6, 8, 9
        };
        double controls[9] {};
        for (int index = 0; index < 9; ++index) {
            controls[index] =
                novelosc::rampedInput(
                    unit, inputIndices[index], sample,
                    inNumSamples, unit->previous[index]);
        }

        const double rootFrequency = novelosc::clampValue(
            controls[0], -sampleRate * 8.0,
            sampleRate * 8.0);
        const double requestedPartials =
            novelosc::clampValue(
                controls[1], 0.0,
                static_cast<double>(unit->maxPartials));
        const int partialLimit = std::min(
            unit->maxPartials,
            static_cast<int>(
                std::ceil(requestedPartials)));
        const double spacing =
            novelosc::clampValue(
                controls[2], 1.0e-5, 64.0);
        const double exponent = std::exp2(
            novelosc::clampValue(
                controls[3], -4.0, 4.0));
        const double tilt =
            novelosc::clampValue(
                controls[4], -4.0, 8.0);
        const double combPeriod =
            novelosc::clampValue(
                controls[5], 0.25, 4096.0);
        const double combDepth =
            novelosc::clampValue(
                controls[6], 0.0, 1.0);
        const double detune =
            novelosc::clampValue(
                controls[7], 0.0, 24.0);
        const double phaseSpread =
            novelosc::clampValue(
                controls[8], 0.0, 1.0);

        double left = 0.0;
        double right = 0.0;
        for (int voice = 0; voice < unit->voices;
             ++voice) {
            const double voicePosition =
                unit->voices > 1
                ? 2.0 * voice / (unit->voices - 1) - 1.0
                : 0.0;
            const double voiceFrequency =
                rootFrequency
                * novelosc::semitoneRatio(
                    voicePosition * detune);
            const double basePhase =
                unit->phases[voice].read();
            double voiceSum = 0.0;
            double normalization = 0.0;
            for (int partial = 0;
                 partial < partialLimit; ++partial) {
                const double activation =
                    novelosc::smoothstep(
                        requestedPartials - partial);
                const double unwarped =
                    1.0 + partial * spacing;
                const double ratio = std::pow(
                    std::max(unwarped, 1.0e-12),
                    exponent);
                const double partialFrequency =
                    std::abs(voiceFrequency * ratio);
                const double nyquistWeight =
                    novelosc::nyquistFade(
                        partialFrequency, nyquist, 0.85);
                if (nyquistWeight <= 0.0)
                    continue;
                const double tiltWeight = std::pow(
                    std::max(ratio, 1.0e-12), -tilt);
                const double combPhase =
                    novelosc::kTwoPi
                    * partial / combPeriod;
                const double combMask =
                    1.0
                    - combDepth
                        * 0.5 * (1.0 - std::cos(combPhase));
                const double weight =
                    activation * nyquistWeight
                    * tiltWeight * combMask;
                const double deterministicOffset =
                    phaseSpread
                    * novelosc::wrapCycles(
                        (partial + 1)
                        * 0.6180339887498948482);
                voiceSum += weight * std::sin(
                    novelosc::kTwoPi
                    * novelosc::wrapCycles(
                        basePhase * ratio
                        + deterministicOffset));
                normalization += std::abs(weight);
            }
            if (normalization > 1.0e-20)
                voiceSum /= normalization;
            else
                voiceSum = 0.0;
            unit->phases[voice].advance(
                voiceFrequency / sampleRate);

            const double panPosition =
                (voicePosition + 1.0) * 0.5;
            left += voiceSum
                * novelosc::equalPowerA(panPosition);
            right += voiceSum
                * novelosc::equalPowerB(panPosition);
        }
        const double outputScale =
            1.0 / std::max(unit->voices, 1);
        leftOutput[sample] =
            novelosc::safeOutput(left * outputScale);
        rightOutput[sample] =
            novelosc::safeOutput(right * outputScale);
    }

    const int inputIndices[9] {
        0, 1, 2, 3, 4, 5, 6, 8, 9
    };
    const float defaults[9] {
        110.f, 128.f, 1.f, 0.f, 1.f,
        1.f, 0.f, 0.f, 0.f
    };
    for (int input = 0; input < 9; ++input)
        novelosc::finishRampedInput(
            unit, inputIndices[input],
            unit->previous[input], defaults[input]);
}

void PartialClusterOsc_nextControlRate(
    PartialClusterOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    const int inputIndices[9] {
        0, 1, 2, 3, 4, 5, 6, 8, 9
    };
    const double defaults[9] {
        110.0, 128.0, 1.0, 0.0, 1.0,
        1.0, 0.0, 0.0, 0.0
    };
    double controls[9] {};
    for (int index = 0; index < 9; ++index) {
        controls[index] = novelosc::finiteOr(
            static_cast<double>(IN0(inputIndices[index])),
            defaults[index]);
    }

    const double sampleRate = unit->mRate->mSampleRate;
    const double nyquist = sampleRate * 0.5;
    const double rootFrequency = novelosc::clampValue(
        controls[0], -sampleRate * 8.0,
        sampleRate * 8.0);
    const double requestedPartials =
        novelosc::clampValue(
            controls[1], 0.0,
            static_cast<double>(unit->maxPartials));
    const int partialLimit = std::min(
        unit->maxPartials,
        static_cast<int>(std::ceil(requestedPartials)));
    const int workLimit =
        std::max(unit->processingLimit, partialLimit);
    const double spacing =
        novelosc::clampValue(
            controls[2], 1.0e-5, 64.0);
    const double exponent = std::exp2(
        novelosc::clampValue(
            controls[3], -4.0, 4.0));
    const double tilt =
        novelosc::clampValue(
            controls[4], -4.0, 8.0);
    const double combPeriod =
        novelosc::clampValue(
            controls[5], 0.25, 4096.0);
    const double combDepth =
        novelosc::clampValue(
            controls[6], 0.0, 1.0);
    const double detune =
        novelosc::clampValue(
            controls[7], 0.0, 24.0);
    const double phaseSpread =
        novelosc::clampValue(
            controls[8], 0.0, 1.0);
    const double phaseSpreadDelta =
        phaseSpread - unit->previousPhaseSpread;

    for (int partial = 0;
         partial < partialLimit; ++partial) {
        const double unwarped =
            1.0 + partial * spacing;
        unit->ratios[partial] = novelosc::finiteOr(
            std::pow(std::max(unwarped, 1.0e-12),
                     exponent),
            1.0e9);
        unit->ratios[partial] = novelosc::clampValue(
            unit->ratios[partial], 1.0e-12, 1.0e9);
    }

    for (int voice = 0; voice < unit->voices; ++voice) {
        const double voicePosition =
            unit->voices > 1
            ? 2.0 * voice / (unit->voices - 1) - 1.0
            : 0.0;
        const double voiceFrequency =
            rootFrequency
            * novelosc::semitoneRatio(
                voicePosition * detune);
        double normalization = 0.0;

        for (int partial = 0; partial < workLimit;
             ++partial) {
            const int index =
                voicePartialIndex(unit, voice, partial);
            unit->targetWeights[index] = 0.0;
            if (partial >= partialLimit)
                continue;
            const double ratio = unit->ratios[partial];
            const double partialFrequency =
                std::abs(voiceFrequency * ratio);
            const double nyquistWeight =
                novelosc::nyquistFade(
                    partialFrequency, nyquist, 0.85);
            if (nyquistWeight <= 0.0)
                continue;
            const double activation =
                novelosc::smoothstep(
                    requestedPartials - partial);
            const double tiltWeight = std::pow(
                std::max(ratio, 1.0e-12), -tilt);
            const double combPhase =
                novelosc::kTwoPi
                * partial / combPeriod;
            const double combMask =
                1.0
                - combDepth
                    * 0.5
                    * (1.0 - std::cos(combPhase));
            const double weight =
                novelosc::finiteOr(
                    activation * nyquistWeight
                        * tiltWeight * combMask,
                    0.0);
            unit->targetWeights[index] = weight;
            normalization += std::abs(weight);
        }

        const double normalizationScale =
            normalization > 1.0e-20
            ? 1.0 / normalization
            : 0.0;
        for (int partial = 0; partial < workLimit;
             ++partial) {
            const int index =
                voicePartialIndex(unit, voice, partial);
            unit->targetWeights[index] *=
                normalizationScale;
            const double ratio = unit->ratios[partial];
            const double deterministicOffset =
                novelosc::wrapCycles(
                    (partial + 1)
                    * 0.6180339887498948482);
            if (unit->currentWeights[index] == 0.0
                && unit->targetWeights[index] != 0.0) {
                const double initialAngle =
                    novelosc::kTwoPi
                    * novelosc::wrapCycles(
                        unit->phases[voice].read()
                            * ratio
                        + phaseSpread
                            * deterministicOffset);
                unit->stateSines[index] =
                    std::sin(initialAngle);
                unit->stateCosines[index] =
                    std::cos(initialAngle);
            } else {
                const double magnitude = std::hypot(
                    unit->stateSines[index],
                    unit->stateCosines[index]);
                if (magnitude > 1.0e-20) {
                    unit->stateSines[index] /= magnitude;
                    unit->stateCosines[index] /= magnitude;
                }
            }

            const double phaseSpreadIncrement =
                phaseSpreadDelta * deterministicOffset
                / std::max(inNumSamples, 1);
            const double phaseIncrement =
                novelosc::clampValue(
                    voiceFrequency * ratio / sampleRate
                        + phaseSpreadIncrement,
                    -16.0, 16.0);
            const double rotationAngle =
                novelosc::kTwoPi * phaseIncrement;
            unit->rotationSines[index] =
                std::sin(rotationAngle);
            unit->rotationCosines[index] =
                std::cos(rotationAngle);
        }
    }

    float* leftOutput = OUT(0);
    float* rightOutput = OUT(1);
    for (int sample = 0; sample < inNumSamples; ++sample) {
        const double interpolation =
            static_cast<double>(sample + 1)
            / std::max(inNumSamples, 1);
        double left = 0.0;
        double right = 0.0;

        for (int voice = 0; voice < unit->voices;
             ++voice) {
            double voiceSum = 0.0;
            for (int partial = 0; partial < workLimit;
                 ++partial) {
                const int index =
                    voicePartialIndex(
                        unit, voice, partial);
                const double weight =
                    unit->currentWeights[index]
                    + (unit->targetWeights[index]
                           - unit->currentWeights[index])
                        * interpolation;
                voiceSum +=
                    weight * unit->stateSines[index];
                const double nextSine =
                    unit->stateSines[index]
                        * unit->rotationCosines[index]
                    + unit->stateCosines[index]
                        * unit->rotationSines[index];
                const double nextCosine =
                    unit->stateCosines[index]
                        * unit->rotationCosines[index]
                    - unit->stateSines[index]
                        * unit->rotationSines[index];
                unit->stateSines[index] = nextSine;
                unit->stateCosines[index] = nextCosine;
            }
            const double voicePosition =
                unit->voices > 1
                ? 2.0 * voice / (unit->voices - 1) - 1.0
                : 0.0;
            const double panPosition =
                (voicePosition + 1.0) * 0.5;
            left += voiceSum
                * novelosc::equalPowerA(panPosition);
            right += voiceSum
                * novelosc::equalPowerB(panPosition);
            const double voiceFrequency =
                rootFrequency
                * novelosc::semitoneRatio(
                    voicePosition * detune);
            unit->phases[voice].advance(
                voiceFrequency / sampleRate);
        }

        const double outputScale =
            1.0 / std::max(unit->voices, 1);
        leftOutput[sample] =
            novelosc::safeOutput(left * outputScale);
        rightOutput[sample] =
            novelosc::safeOutput(right * outputScale);
    }

    for (int voice = 0; voice < unit->voices; ++voice) {
        for (int partial = 0; partial < workLimit;
             ++partial) {
            const int index =
                voicePartialIndex(unit, voice, partial);
            unit->currentWeights[index] =
                unit->targetWeights[index];
        }
    }
    unit->processingLimit = partialLimit;
    unit->previousPhaseSpread = phaseSpread;
}

void PartialClusterOsc_Ctor(PartialClusterOsc* unit)
{
    SETCALC(PartialClusterOsc_nextAudioRate);
    unit->ratios = nullptr;
    unit->stateSines = nullptr;
    unit->stateCosines = nullptr;
    unit->rotationSines = nullptr;
    unit->rotationCosines = nullptr;
    unit->currentWeights = nullptr;
    unit->targetWeights = nullptr;
    unit->previousPhaseSpread = 0.0;
    unit->processingLimit = 0;
    unit->voices = 1;
    unit->maxPartials = 1;
    unit->audioRateMode = false;
    unit->failed = false;
    unit->errorReported = false;
    for (auto& phase : unit->phases)
        phase = {};
    const bool valid =
        novelosc::exactInitInt(
            IN0(7), 1, kMaximumVoices, &unit->voices)
        && novelosc::exactInitInt(
            IN0(10), 1, 4096, &unit->maxPartials);
    if (!valid) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported, "PartialClusterOsc",
            "voices or maxPartials is outside its initialization range.");
        return;
    }
    const int inputIndices[9] {
        0, 1, 2, 3, 4, 5, 6, 8, 9
    };
    const float defaults[9] {
        110.f, 128.f, 1.f, 0.f, 1.f,
        1.f, 0.f, 0.f, 0.f
    };
    for (int input = 0; input < 9; ++input)
        unit->previous[input] =
            novelosc::finiteOr(
                IN0(inputIndices[input]), defaults[input]);
    for (int input = 0; input < 9; ++input) {
        if (INRATE(inputIndices[input]) == calc_FullRate)
            unit->audioRateMode = true;
    }

    if (!unit->audioRateMode) {
        const std::size_t partialCount =
            static_cast<std::size_t>(unit->maxPartials);
        const std::size_t voicePartialCount =
            partialCount * unit->voices;
        unit->ratios = novelosc::allocateRT<double>(
            unit->mWorld, partialCount);
        unit->stateSines = novelosc::allocateRT<double>(
            unit->mWorld, voicePartialCount);
        unit->stateCosines = novelosc::allocateRT<double>(
            unit->mWorld, voicePartialCount);
        unit->rotationSines =
            novelosc::allocateRT<double>(
                unit->mWorld, voicePartialCount);
        unit->rotationCosines =
            novelosc::allocateRT<double>(
                unit->mWorld, voicePartialCount);
        unit->currentWeights =
            novelosc::allocateRT<double>(
                unit->mWorld, voicePartialCount);
        unit->targetWeights =
            novelosc::allocateRT<double>(
                unit->mWorld, voicePartialCount);
        if (!unit->ratios || !unit->stateSines
            || !unit->stateCosines
            || !unit->rotationSines
            || !unit->rotationCosines
            || !unit->currentWeights
            || !unit->targetWeights) {
            unit->failed = true;
            novelosc::clearAndReport(
                unit, unit->errorReported,
                "PartialClusterOsc",
                "real-time oscillator-bank allocation failed.");
            return;
        }
        std::fill_n(
            unit->ratios, partialCount, 1.0);
        std::fill_n(
            unit->stateSines, voicePartialCount, 0.0);
        std::fill_n(
            unit->stateCosines, voicePartialCount, 1.0);
        std::fill_n(
            unit->rotationSines, voicePartialCount, 0.0);
        std::fill_n(
            unit->rotationCosines, voicePartialCount, 1.0);
        std::fill_n(
            unit->currentWeights,
            voicePartialCount, 0.0);
        std::fill_n(
            unit->targetWeights,
            voicePartialCount, 0.0);
        unit->previousPhaseSpread =
            novelosc::clampValue(
                static_cast<double>(
                    novelosc::finiteOr(IN0(9), 0.f)),
                0.0, 1.0);
        SETCALC(PartialClusterOsc_nextControlRate);
    }
    ClearUnitOutputs(unit, 1);
}

void PartialClusterOsc_Dtor(PartialClusterOsc* unit)
{
    novelosc::releaseRT(unit->mWorld, unit->ratios);
    novelosc::releaseRT(unit->mWorld, unit->stateSines);
    novelosc::releaseRT(
        unit->mWorld, unit->stateCosines);
    novelosc::releaseRT(
        unit->mWorld, unit->rotationSines);
    novelosc::releaseRT(
        unit->mWorld, unit->rotationCosines);
    novelosc::releaseRT(
        unit->mWorld, unit->currentWeights);
    novelosc::releaseRT(
        unit->mWorld, unit->targetWeights);
}

} // namespace

void registerPartialClusterOsc()
{
    DefineDtorCantAliasUnit(PartialClusterOsc);
}
