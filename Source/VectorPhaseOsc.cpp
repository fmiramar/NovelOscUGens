// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/PhaseCore.hpp"
#include "DSP/PhaseSegments.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct VectorPhaseOsc : public Unit {
    novelosc::PhaseCore phase;
    novelosc::OversamplingDecimator decimator;
    float previousFrequency = 440.f;
    float previousPhaseMod = 0.f;
    float previousSync = 0.f;
    int oversample = 1;
    int count = 1;
    bool failed = false;
};

void VectorPhaseOsc_next(
    VectorPhaseOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    const double nyquist = sampleRate * 0.5;
    float* output = OUT(0);
    double sourceX[novelosc::kMaximumPhaseBreakpoints] {};
    double sourceY[novelosc::kMaximumPhaseBreakpoints] {};
    double x[novelosc::kMaximumPhaseBreakpoints] {};
    double y[novelosc::kMaximumPhaseBreakpoints] {};

    for (int sample = 0; sample < inNumSamples; ++sample) {
        const double frequency = novelosc::rampedInput(
            unit, 0, sample, inNumSamples,
            unit->previousFrequency, 440.f);
        const double phaseMod = novelosc::rampedInput(
            unit, 1, sample, inNumSamples,
            unit->previousPhaseMod, 0.f)
            * novelosc::kInvTwoPi;
        const float sync = static_cast<float>(
            novelosc::inputSample(unit, 2, sample, 0.f));
        if (sync > 0.f && unit->previousSync <= 0.f)
            unit->phase.reset();
        unit->previousSync = sync;

        for (int point = 0; point < unit->count; ++point) {
            sourceX[point] = novelosc::inputSample(
                unit, 5 + point, sample, 0.5f);
            sourceY[point] = novelosc::inputSample(
                unit, 5 + unit->count + point,
                sample, 0.5f);
        }
        novelosc::sanitizePhaseBreakpoints(
            sourceX, sourceY, unit->count, x, y);
        const double slope =
            novelosc::maximumPhaseSegmentSlope(
                x, y, unit->count);
        const double fade = novelosc::nyquistFade(
            std::abs(frequency) * slope, nyquist, 0.82);
        const double increment =
            novelosc::clampValue(
                frequency / (sampleRate * unit->oversample),
                -8.0, 8.0);
        double value = 0.0;
        for (int substep = 0;
             substep < unit->oversample; ++substep) {
            const double mapped = novelosc::mapPhaseSegments(
                unit->phase.read(phaseMod),
                x, y, unit->count);
            value = unit->decimator.process(
                std::sin(novelosc::kTwoPi * mapped) * fade);
            unit->phase.advance(increment);
        }
        output[sample] = novelosc::safeOutput(value);
    }
    novelosc::finishRampedInput(
        unit, 0, unit->previousFrequency, 440.f);
    novelosc::finishRampedInput(
        unit, 1, unit->previousPhaseMod, 0.f);
}

void VectorPhaseOsc_Ctor(VectorPhaseOsc* unit)
{
    SETCALC(VectorPhaseOsc_next);
    unit->phase = {};
    unit->decimator = {};
    unit->previousFrequency = novelosc::finiteOr(IN0(0), 440.f);
    unit->previousPhaseMod = novelosc::finiteOr(IN0(1), 0.f);
    unit->previousSync = novelosc::finiteOr(IN0(2), 0.f);
    unit->oversample = 1;
    unit->count = 1;
    unit->failed = false;

    const bool valid =
        novelosc::exactInitInt(IN0(3), 1, 4, &unit->oversample)
        && (unit->oversample == 1
            || unit->oversample == 2
            || unit->oversample == 4)
        && novelosc::exactInitInt(
            IN0(4), 1, novelosc::kMaximumPhaseBreakpoints,
            &unit->count);
    if (!valid
        || static_cast<int>(unit->mNumInputs)
            != 5 + 2 * unit->count) {
        unit->failed = true;
        Print("VectorPhaseOsc: invalid breakpoint count or oversampling factor.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    unit->decimator.configure(unit->oversample);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerVectorPhaseOsc()
{
    DefineSimpleCantAliasUnit(VectorPhaseOsc);
}
