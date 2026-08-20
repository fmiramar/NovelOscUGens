// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/OscillatorShapes.hpp"
#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kDynamicInputs = 25;

struct PMNetworkOsc : public Unit {
    novelosc::PhaseCore phases[3];
    novelosc::OversamplingDecimator filters[3];
    double previousOutputs[3] {};
    float previous[kDynamicInputs] {};
    int waveforms[3] {};
    int oversample = 1;
    bool failed = false;
    bool errorReported = false;
};

void PMNetworkOsc_next(
    PMNetworkOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    float* outputs[3] { OUT(0), OUT(1), OUT(2) };

    for (int sample = 0; sample < inNumSamples; ++sample) {
        double controls[kDynamicInputs] {};
        for (int input = 0; input < kDynamicInputs;
             ++input) {
            controls[input] =
                novelosc::rampedInput(
                    unit, input, sample, inNumSamples,
                    unit->previous[input]);
        }

        double filtered[3] {};
        for (int substep = 0; substep < unit->oversample;
             ++substep) {
            double nextOutputs[3] {};
            for (int row = 0; row < 3; ++row) {
                double phaseModulation = 0.0;
                double frequencyModulation = 0.0;
                for (int column = 0; column < 3;
                     ++column) {
                    phaseModulation +=
                        controls[
                            4 + novelosc::rowMajorIndex(
                                row, column, 3)]
                        * unit->previousOutputs[column];
                    frequencyModulation +=
                        controls[
                            13 + novelosc::rowMajorIndex(
                                row, column, 3)]
                        * unit->previousOutputs[column];
                }
                const double operatorFrequency =
                    novelosc::clampValue(
                        controls[0] * controls[1 + row]
                            + frequencyModulation,
                        -sampleRate * unit->oversample * 8.0,
                        sampleRate * unit->oversample * 8.0);
                const double increment =
                    operatorFrequency
                    / (sampleRate * unit->oversample);
                unit->phases[row].advance(increment);
                const double readPhase =
                    unit->phases[row].read(
                        phaseModulation
                        * novelosc::kInvTwoPi);
                const double amplitude =
                    novelosc::clampValue(
                        controls[22 + row], -4.0, 4.0);
                nextOutputs[row] =
                    novelosc::oscillatorShape(
                        unit->waveforms[row], readPhase,
                        increment)
                    * amplitude;
            }
            for (int row = 0; row < 3; ++row) {
                unit->previousOutputs[row] =
                    novelosc::finiteOr(
                        nextOutputs[row], 0.0);
                filtered[row] =
                    unit->filters[row].process(
                        unit->previousOutputs[row]);
            }
        }
        for (int output = 0; output < 3; ++output)
            outputs[output][sample] =
                novelosc::safeOutput(filtered[output]);
    }

    const float defaults[kDynamicInputs] {
        110.f,
        1.f, 2.f, 3.f,
        0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
        1.f, 1.f, 1.f
    };
    for (int input = 0; input < kDynamicInputs; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            defaults[input]);
}

void PMNetworkOsc_Ctor(PMNetworkOsc* unit)
{
    SETCALC(PMNetworkOsc_next);
    unit->oversample = 1;
    unit->failed = false;
    unit->errorReported = false;
    for (int index = 0; index < 3; ++index) {
        unit->phases[index] = {};
        unit->filters[index] = {};
        unit->previousOutputs[index] = 0.0;
        unit->waveforms[index] = 0;
    }
    bool valid = true;
    for (int index = 0; index < 3; ++index) {
        valid = valid && novelosc::exactInitInt(
            IN0(25 + index), 0, 3,
            &unit->waveforms[index]);
    }
    unit->oversample =
        novelosc::clampOversample(IN0(28));
    if (!valid) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported, "PMNetworkOsc",
            "waveforms must contain three integers from 0 to 3.");
        return;
    }
    for (auto& filter : unit->filters)
        filter.configure(unit->oversample);

    const float defaults[kDynamicInputs] {
        110.f,
        1.f, 2.f, 3.f,
        0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
        1.f, 1.f, 1.f
    };
    for (int input = 0; input < kDynamicInputs; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerPMNetworkOsc()
{
    DefineSimpleCantAliasUnit(PMNetworkOsc);
}
