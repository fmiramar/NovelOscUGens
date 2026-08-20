// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/PhaseCore.hpp"
#include "DSP/RippleGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct RippleFormantOsc : public Unit {
    novelosc::PhaseCore basePhase;
    novelosc::RippleState rippleA;
    novelosc::RippleState rippleB;
    novelosc::OversamplingDecimator baseFilter;
    novelosc::OversamplingDecimator rippleAFilter;
    novelosc::OversamplingDecimator rippleBFilter;
    float previous[11] {};
    int oversample = 2;
    bool failed = false;
};

void RippleFormantOsc_next(
    RippleFormantOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    const double oversampledRate = sampleRate * unit->oversample;
    const double interval = 1.0 / oversampledRate;
    const double nyquist = sampleRate * 0.5;
    float* baseOutput = OUT(0);
    float* rippleAOutput = OUT(1);
    float* rippleBOutput = OUT(2);
    float* mixOutput = OUT(3);

    const float defaults[11] {
        110.f, 700.f, 1200.f, 0.01f, 0.008f,
        0.f, 0.f, 0.5f, 0.2f, 1.f, 0.f
    };
    for (int sample = 0; sample < inNumSamples; ++sample) {
        double control[11] {};
        for (int input = 0; input < 11; ++input)
            control[input] = novelosc::rampedInput(
                unit, input, sample, inNumSamples,
                unit->previous[input], defaults[input]);
        const double frequency = novelosc::clampValue(
            control[0], -sampleRate * 8.0, sampleRate * 8.0);
        const double basePeriod = std::abs(frequency) > 1.0e-12
            ? 1.0 / std::abs(frequency)
            : 1.0e12;
        const double direction = frequency < 0.0 ? -1.0 : 1.0;
        const double phaseOffset =
            control[10] * novelosc::kInvTwoPi;
        const double increment =
            frequency / oversampledRate;
        double base = 0.0;
        double rippleA = 0.0;
        double rippleB = 0.0;
        for (int substep = 0;
             substep < unit->oversample; ++substep) {
            base = unit->baseFilter.process(
                std::sin(novelosc::kTwoPi
                    * unit->basePhase.read(phaseOffset)));
            rippleA = unit->rippleAFilter.process(
                novelosc::processRipple(
                    unit->rippleA, control[1], control[3],
                    control[5], basePeriod, direction,
                    interval, nyquist));
            rippleB = unit->rippleBFilter.process(
                novelosc::processRipple(
                    unit->rippleB, control[2], control[4],
                    control[6], basePeriod, direction,
                    interval, nyquist));
            if (unit->basePhase.advance(increment)) {
                unit->rippleA.reset();
                unit->rippleB.reset();
            }
        }
        const double balance =
            novelosc::clampValue(control[7], 0.0, 1.0);
        const double combinedRipple =
            rippleA * novelosc::equalPowerA(balance)
            + rippleB * novelosc::equalPowerB(balance);
        baseOutput[sample] = novelosc::safeOutput(base);
        rippleAOutput[sample] = novelosc::safeOutput(rippleA);
        rippleBOutput[sample] = novelosc::safeOutput(rippleB);
        mixOutput[sample] = novelosc::safeOutput(
            control[8] * base + control[9] * combinedRipple);
    }
    for (int input = 0; input < 11; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            defaults[input]);
}

void RippleFormantOsc_Ctor(RippleFormantOsc* unit)
{
    SETCALC(RippleFormantOsc_next);
    unit->basePhase = {};
    unit->rippleA = {};
    unit->rippleB = {};
    unit->baseFilter = {};
    unit->rippleAFilter = {};
    unit->rippleBFilter = {};
    std::fill(unit->previous, unit->previous + 11, 0.f);
    unit->oversample = 2;
    unit->failed = false;
    const bool valid =
        novelosc::exactInitInt(IN0(11), 1, 4, &unit->oversample)
        && (unit->oversample == 1
            || unit->oversample == 2
            || unit->oversample == 4);
    if (!valid) {
        unit->failed = true;
        Print("RippleFormantOsc: oversample must be 1, 2, or 4.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    unit->baseFilter.configure(unit->oversample);
    unit->rippleAFilter.configure(unit->oversample);
    unit->rippleBFilter.configure(unit->oversample);
    const float defaults[11] {
        110.f, 700.f, 1200.f, 0.01f, 0.008f,
        0.f, 0.f, 0.5f, 0.2f, 1.f, 0.f
    };
    for (int input = 0; input < 11; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerRippleFormantOsc()
{
    DefineSimpleCantAliasUnit(RippleFormantOsc);
}
