// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct TZSplitOsc : public Unit {
    novelosc::PhaseCore phase;
    novelosc::OversamplingDecimator filters[4];
    float previous[5] {};
    float previousReset = 0.f;
    float previousFlip = 0.f;
    int oversample = 1;
    bool failed = false;
    bool errorReported = false;
};

double applyDrive(double value, double drive)
{
    const double amount =
        novelosc::clampValue(drive, 1.0, 16.0) - 1.0;
    if (amount < 1.0e-6)
        return value;
    const double denominator = std::tanh(amount);
    return denominator > 1.0e-12
        ? std::tanh(amount * value) / denominator
        : value;
}

void TZSplitOsc_next(TZSplitOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    const double sampleRate = unit->mRate->mSampleRate;
    float* outputs[4] {
        OUT(0), OUT(1), OUT(2), OUT(3)
    };
    for (int sample = 0; sample < inNumSamples; ++sample) {
        const double frequency =
            novelosc::rampedInput(
                unit, 0, sample, inNumSamples,
                unit->previous[0]);
        const double linearFM =
            novelosc::rampedInput(
                unit, 1, sample, inNumSamples,
                unit->previous[1]);
        const double phaseMod =
            novelosc::rampedInput(
                unit, 2, sample, inNumSamples,
                unit->previous[2])
            * novelosc::kInvTwoPi;
        const double symmetry = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 3, sample, inNumSamples,
                unit->previous[3], 0.5f)),
            0.02, 0.98);
        const double drive = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 4, sample, inNumSamples,
                unit->previous[4], 1.f)),
            1.0, 16.0);
        const float reset =
            novelosc::inputSample(
                unit, 5, sample, 0.f);
        const float flip =
            novelosc::inputSample(
                unit, 6, sample, 0.f);
        if (reset > 0.f && unit->previousReset <= 0.f)
            unit->phase.reset();
        if (flip > 0.f && unit->previousFlip <= 0.f)
            unit->phase.flip();
        unit->previousReset = reset;
        unit->previousFlip = flip;

        double filtered[4] {};
        const double totalFrequency = novelosc::clampValue(
            frequency + linearFM,
            -sampleRate * unit->oversample * 8.0,
            sampleRate * unit->oversample * 8.0);
        const double increment =
            totalFrequency
            / (sampleRate * unit->oversample);
        for (int substep = 0; substep < unit->oversample;
             ++substep) {
            const double phase =
                unit->phase.read(phaseMod);
            const double shiftedPhase =
                novelosc::wrapCycles(phase + 0.5);
            const double full = applyDrive(
                novelosc::asymmetricTriangle(
                    phase, symmetry, increment),
                drive);
            const double shifted = applyDrive(
                novelosc::asymmetricTriangle(
                    shiftedPhase, symmetry, increment),
                drive);
            const double values[4] {
                std::sin(novelosc::kTwoPi * phase),
                0.5 * (full + shifted),
                0.5 * (full - shifted),
                full
            };
            for (int output = 0; output < 4; ++output)
                filtered[output] =
                    unit->filters[output].process(
                        values[output]);
            unit->phase.advance(increment);
        }
        for (int output = 0; output < 4; ++output)
            outputs[output][sample] =
                novelosc::safeOutput(filtered[output]);
    }

    for (int input = 0; input < 5; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            input == 3 ? 0.5f
                       : (input == 4 ? 1.f : 0.f));
}

void TZSplitOsc_Ctor(TZSplitOsc* unit)
{
    SETCALC(TZSplitOsc_next);
    unit->phase = {};
    unit->previousReset = 0.f;
    unit->previousFlip = 0.f;
    unit->oversample = 1;
    unit->failed = false;
    unit->errorReported = false;
    unit->oversample =
        novelosc::clampOversample(IN0(7));
    for (auto& filter : unit->filters) {
        filter = {};
        filter.configure(unit->oversample);
    }
    for (int input = 0; input < 5; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), 0.f);
    unit->previousReset =
        novelosc::finiteOr(IN0(5), 0.f);
    unit->previousFlip =
        novelosc::finiteOr(IN0(6), 0.f);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerTZSplitOsc()
{
    DefineSimpleCantAliasUnit(TZSplitOsc);
}
