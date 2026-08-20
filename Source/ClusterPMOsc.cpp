// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kDynamicInputs = 33;

struct ClusterPMOsc : public Unit {
    novelosc::PhaseCore phases[4];
    novelosc::OversamplingDecimator filters[4];
    double previousOutputs[4] {};
    float previous[kDynamicInputs] {};
    int oversample = 1;
    bool failed = false;
    bool errorReported = false;
};

void ClusterPMOsc_next(
    ClusterPMOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    const double nyquist = sampleRate * 0.5;
    float* outputs[4] {
        OUT(0), OUT(1), OUT(2), OUT(3)
    };

    for (int sample = 0; sample < inNumSamples; ++sample) {
        double controls[kDynamicInputs] {};
        for (int input = 0; input < kDynamicInputs;
             ++input) {
            controls[input] =
                novelosc::rampedInput(
                    unit, input, sample, inNumSamples,
                    unit->previous[input]);
        }

        double filtered[4] {};
        for (int substep = 0; substep < unit->oversample;
             ++substep) {
            double nextOutputs[4] {};
            for (int row = 0; row < 4; ++row) {
                double phaseModulation = 0.0;
                for (int column = 0; column < 4;
                     ++column) {
                    phaseModulation +=
                        controls[
                            13 + novelosc::rowMajorIndex(
                                row, column, 4)]
                        * unit->previousOutputs[column];
                }
                const double operatorFrequency =
                    novelosc::clampValue(
                        controls[0] * controls[1 + row],
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

                double cluster = 0.0;
                double normalization = 0.0;
                for (int component = 0; component < 4;
                     ++component) {
                    const double ratio = novelosc::clampValue(
                        controls[5 + component],
                        -64.0, 64.0);
                    const double componentFrequency =
                        std::abs(operatorFrequency * ratio);
                    const double fade = novelosc::nyquistFade(
                        componentFrequency, nyquist, 0.85);
                    const double amplitude =
                        novelosc::clampValue(
                            controls[9 + component],
                            -8.0, 8.0)
                        * fade;
                    cluster += amplitude * std::sin(
                        novelosc::kTwoPi
                        * novelosc::wrapCycles(
                            readPhase * ratio));
                    normalization += std::abs(amplitude);
                }
                if (normalization > 1.0e-20)
                    cluster /= normalization;
                else
                    cluster = 0.0;
                nextOutputs[row] =
                    cluster
                    * novelosc::clampValue(
                        controls[29 + row], -4.0, 4.0);
            }
            for (int row = 0; row < 4; ++row) {
                unit->previousOutputs[row] =
                    novelosc::finiteOr(
                        nextOutputs[row], 0.0);
                filtered[row] =
                    unit->filters[row].process(
                        unit->previousOutputs[row]);
            }
        }
        for (int output = 0; output < 4; ++output)
            outputs[output][sample] =
                novelosc::safeOutput(filtered[output]);
    }

    const float defaults[kDynamicInputs] {
        110.f,
        1.f, 2.f, 3.f, 4.f,
        1.f, 1.01f, 2.f, 3.f,
        1.f, 0.25f, 0.15f, 0.1f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        1.f, 1.f, 1.f, 1.f
    };
    for (int input = 0; input < kDynamicInputs; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            defaults[input]);
}

void ClusterPMOsc_Ctor(ClusterPMOsc* unit)
{
    SETCALC(ClusterPMOsc_next);
    unit->oversample = 1;
    unit->failed = false;
    unit->errorReported = false;
    for (int index = 0; index < 4; ++index) {
        unit->phases[index] = {};
        unit->filters[index] = {};
        unit->previousOutputs[index] = 0.0;
    }
    unit->oversample =
        novelosc::clampOversample(IN0(33));
    for (auto& filter : unit->filters)
        filter.configure(unit->oversample);
    const float defaults[kDynamicInputs] {
        110.f,
        1.f, 2.f, 3.f, 4.f,
        1.f, 1.01f, 2.f, 3.f,
        1.f, 0.25f, 0.15f, 0.1f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        1.f, 1.f, 1.f, 1.f
    };
    for (int input = 0; input < kDynamicInputs; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerClusterPMOsc()
{
    DefineSimpleCantAliasUnit(ClusterPMOsc);
}
