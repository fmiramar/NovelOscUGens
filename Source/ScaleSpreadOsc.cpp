// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/BufferValidation.hpp"
#include "DSP/MathUtils.hpp"
#include "DSP/OscillatorShapes.hpp"
#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kMaximumVoices = 32;

struct ScaleSpreadOsc : public Unit {
    novelosc::PhaseCore phases[kMaximumVoices][2];
    double previousVoiceOutputs[kMaximumVoices] {};
    float previous[6] {};
    int bufferNumber = -1;
    int voices = 1;
    int waveform = 0;
    int scaleSize = 0;
    SndBuf* buffer = nullptr;
    float* ratios = nullptr;
    bool failed = false;
    bool errorReported = false;
};

bool validateScale(const SndBuf* buffer)
{
    if (!buffer || !buffer->data || buffer->channels != 1
        || buffer->samples < 2 || buffer->samples > 16384)
        return false;
    if (!std::isfinite(buffer->data[0])
        || std::abs(buffer->data[0] - 1.f) > 1.0e-5f)
        return false;
    float previous = buffer->data[0];
    for (int index = 1;
         index < static_cast<int>(buffer->samples);
         ++index) {
        const float value = buffer->data[index];
        if (!std::isfinite(value) || value <= previous)
            return false;
        previous = value;
    }
    return true;
}

void ScaleSpreadOsc_next(
    ScaleSpreadOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    if (!novelosc::bufferIdentityMatches(
            unit, unit->bufferNumber, unit->buffer,
            unit->ratios, unit->scaleSize)) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported, "ScaleSpreadOsc",
            "the prepared scale buffer disappeared or changed.");
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    const double sampleRate = unit->mRate->mSampleRate;
    float* leftOutput = OUT(0);
    float* rightOutput = OUT(1);
    {
        SndBuf* buffer = unit->buffer;
        LOCK_SNDBUF_SHARED(buffer);
        if (!buffer->data
            || buffer->data != unit->ratios) {
            unit->failed = true;
            novelosc::clearAndReport(
                unit, unit->errorReported,
                "ScaleSpreadOsc",
                "the prepared scale buffer became unavailable.");
            ClearUnitOutputs(unit, inNumSamples);
            return;
        }

        for (int sample = 0; sample < inNumSamples;
             ++sample) {
            const double rootFrequency =
                novelosc::rampedInput(
                    unit, 1, sample, inNumSamples,
                    unit->previous[0], 110.f);
            const double center =
                novelosc::rampedInput(
                    unit, 2, sample, inNumSamples,
                    unit->previous[1]);
            const double spread =
                novelosc::rampedInput(
                    unit, 3, sample, inNumSamples,
                    unit->previous[2], 1.f);
            double period = novelosc::rampedInput(
                unit, 5, sample, inNumSamples,
                unit->previous[3], 2.f);
            const double minimumPeriod =
                unit->ratios[unit->scaleSize - 1]
                * 1.000001;
            period = novelosc::clampValue(
                period, minimumPeriod, 16.0);
            const double modulation =
                novelosc::clampValue(
                    static_cast<double>(
                        novelosc::rampedInput(
                            unit, 6, sample, inNumSamples,
                            unit->previous[4])),
                    -16.0, 16.0);
            const double stereo = novelosc::clampValue(
                static_cast<double>(novelosc::rampedInput(
                    unit, 7, sample, inNumSamples,
                    unit->previous[5], 1.f)),
                0.0, 1.0);

            double left = 0.0;
            double right = 0.0;
            double newVoiceOutputs[kMaximumVoices] {};
            for (int voice = 0; voice < unit->voices;
                 ++voice) {
                const double centeredVoice =
                    voice
                    - 0.5 * (unit->voices - 1);
                const double coordinate =
                    center + centeredVoice * spread;
                const double periodIndex =
                    std::floor(
                        coordinate / unit->scaleSize);
                const double localCoordinate =
                    coordinate
                    - periodIndex * unit->scaleSize;
                const int lowerDegree =
                    novelosc::clampValue(
                        static_cast<int>(
                            std::floor(localCoordinate)),
                        0, unit->scaleSize - 1);
                const double fraction =
                    localCoordinate - lowerDegree;
                const int upperDegree =
                    lowerDegree + 1 < unit->scaleSize
                    ? lowerDegree + 1
                    : 0;
                const double lowerPeriod =
                    novelosc::clampValue(
                        periodIndex, -64.0, 64.0);
                const double upperPeriod =
                    lowerPeriod
                    + (upperDegree == 0 ? 1.0 : 0.0);
                const double lowerRatio =
                    unit->ratios[lowerDegree]
                    * std::pow(period, lowerPeriod);
                const double upperRatio =
                    unit->ratios[upperDegree]
                    * std::pow(period, upperPeriod);
                const double lowerFrequency =
                    novelosc::clampValue(
                        rootFrequency * lowerRatio,
                        -sampleRate * 8.0,
                        sampleRate * 8.0);
                const double upperFrequency =
                    novelosc::clampValue(
                        rootFrequency * upperRatio,
                        -sampleRate * 8.0,
                        sampleRate * 8.0);
                const int neighbor =
                    voice == 0 ? unit->voices - 1
                               : voice - 1;
                const double phaseModulation =
                    modulation
                    * unit->previousVoiceOutputs[neighbor]
                    * novelosc::kInvTwoPi;
                const double lower =
                    novelosc::oscillatorShape(
                        unit->waveform,
                        unit->phases[voice][0].read(
                            phaseModulation),
                        lowerFrequency / sampleRate);
                const double upper =
                    novelosc::oscillatorShape(
                        unit->waveform,
                        unit->phases[voice][1].read(
                            phaseModulation),
                        upperFrequency / sampleRate);
                unit->phases[voice][0].advance(
                    lowerFrequency / sampleRate);
                unit->phases[voice][1].advance(
                    upperFrequency / sampleRate);
                const double voiceOutput =
                    lower
                        * novelosc::equalPowerA(fraction)
                    + upper
                        * novelosc::equalPowerB(fraction);
                newVoiceOutputs[voice] = voiceOutput;

                const double normalizedPosition =
                    unit->voices > 1
                    ? (2.0 * voice / (unit->voices - 1)
                       - 1.0)
                    : 0.0;
                const double pan =
                    normalizedPosition * stereo;
                const double panPosition =
                    (pan + 1.0) * 0.5;
                left += voiceOutput
                    * novelosc::equalPowerA(panPosition);
                right += voiceOutput
                    * novelosc::equalPowerB(panPosition);
            }
            for (int voice = 0; voice < unit->voices;
                 ++voice)
                unit->previousVoiceOutputs[voice] =
                    newVoiceOutputs[voice];
            const double scale =
                1.0 / std::max(unit->voices, 1);
            leftOutput[sample] =
                novelosc::safeOutput(left * scale);
            rightOutput[sample] =
                novelosc::safeOutput(right * scale);
        }
    }

    const int inputIndices[6] { 1, 2, 3, 5, 6, 7 };
    const float defaults[6] {
        110.f, 0.f, 1.f, 2.f, 0.f, 1.f
    };
    for (int input = 0; input < 6; ++input)
        novelosc::finishRampedInput(
            unit, inputIndices[input],
            unit->previous[input], defaults[input]);
}

void ScaleSpreadOsc_Ctor(ScaleSpreadOsc* unit)
{
    SETCALC(ScaleSpreadOsc_next);
    unit->bufferNumber = -1;
    unit->voices = 1;
    unit->waveform = 0;
    unit->scaleSize = 0;
    unit->buffer = nullptr;
    unit->ratios = nullptr;
    unit->failed = false;
    unit->errorReported = false;
    for (int voice = 0; voice < kMaximumVoices; ++voice) {
        unit->phases[voice][0] = {};
        unit->phases[voice][1] = {};
        unit->previousVoiceOutputs[voice] = 0.0;
    }
    bool valid =
        novelosc::exactInitInt(
            IN0(0), 0,
            novelosc::kMaxExactFloatInteger - 1,
            &unit->bufferNumber)
        && novelosc::exactInitInt(
            IN0(4), 1, kMaximumVoices, &unit->voices)
        && novelosc::exactInitInt(
            IN0(8), 0, 2, &unit->waveform);
    if (valid)
        unit->buffer = novelosc::resolveBuffer(
            unit, unit->bufferNumber);
    valid = valid && validateScale(unit->buffer);
    if (!valid) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported, "ScaleSpreadOsc",
            "scale buffer, voices, or waveform is invalid.");
        return;
    }
    unit->ratios = unit->buffer->data;
    unit->scaleSize =
        static_cast<int>(unit->buffer->samples);
    const int inputIndices[6] { 1, 2, 3, 5, 6, 7 };
    const float defaults[6] {
        110.f, 0.f, 1.f, 2.f, 0.f, 1.f
    };
    for (int input = 0; input < 6; ++input)
        unit->previous[input] =
            novelosc::finiteOr(
                IN0(inputIndices[input]), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerScaleSpreadOsc()
{
    DefineSimpleCantAliasUnit(ScaleSpreadOsc);
}
