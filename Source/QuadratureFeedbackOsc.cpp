// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/HopfCore.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct QuadratureFeedbackOsc : public Unit {
    novelosc::HopfState state;
    novelosc::OversamplingDecimator xFilter;
    novelosc::OversamplingDecimator yFilter;
    float previous[9] {};
    int oversample = 4;
    bool failed = false;
};

void QuadratureFeedbackOsc_next(
    QuadratureFeedbackOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    const double interval =
        1.0 / (sampleRate * unit->oversample);
    float* phase0 = OUT(0);
    float* phase90 = OUT(1);
    float* phase180 = OUT(2);
    float* phase270 = OUT(3);
    const float defaults[9] {
        110.f, 0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 1.f
    };

    for (int sample = 0; sample < inNumSamples; ++sample) {
        double control[9] {};
        for (int input = 0; input < 9; ++input)
            control[input] = novelosc::rampedInput(
                unit, input, sample, inNumSamples,
                unit->previous[input], defaults[input]);
        const double frequency = novelosc::clampValue(
            control[0], -sampleRate * 2.0, sampleRate * 2.0);
        double feedback[4] {};
        for (int index = 0; index < 4; ++index)
            feedback[index] =
                novelosc::clampValue(control[index + 2], -8.0, 8.0);
        const double injection =
            novelosc::clampValue(control[6], -8.0, 8.0);
        const double stability =
            novelosc::clampValue(control[7], 0.0, 16.0);
        const double drive =
            novelosc::clampValue(control[8], 0.0, 16.0);
        double x = unit->state.x;
        double y = unit->state.y;
        for (int substep = 0;
             substep < unit->oversample; ++substep) {
            novelosc::advanceHopfRK4(
                unit->state, interval,
                novelosc::kTwoPi * frequency,
                stability, drive, feedback, injection,
                novelosc::clampValue(control[1], -16.0, 16.0));
            x = unit->xFilter.process(unit->state.x);
            y = unit->yFilter.process(unit->state.y);
        }
        phase0[sample] = novelosc::safeOutput(x);
        phase90[sample] = novelosc::safeOutput(y);
        phase180[sample] = novelosc::safeOutput(-x);
        phase270[sample] = novelosc::safeOutput(-y);
    }
    for (int input = 0; input < 9; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            defaults[input]);
}

void QuadratureFeedbackOsc_Ctor(
    QuadratureFeedbackOsc* unit)
{
    SETCALC(QuadratureFeedbackOsc_next);
    unit->state = {};
    unit->xFilter = {};
    unit->yFilter = {};
    std::fill(unit->previous, unit->previous + 9, 0.f);
    unit->oversample = 4;
    unit->failed = false;
    const bool valid =
        novelosc::exactInitInt(IN0(9), 1, 8, &unit->oversample)
        && (unit->oversample == 1
            || unit->oversample == 2
            || unit->oversample == 4
            || unit->oversample == 8);
    if (!valid) {
        unit->failed = true;
        Print("QuadratureFeedbackOsc: oversample must be 1, 2, 4, or 8.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    unit->xFilter.configure(unit->oversample);
    unit->yFilter.configure(unit->oversample);
    const float defaults[9] {
        110.f, 0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 1.f
    };
    for (int input = 0; input < 9; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerQuadratureFeedbackOsc()
{
    DefineSimpleCantAliasUnit(QuadratureFeedbackOsc);
}
