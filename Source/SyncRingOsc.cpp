// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/OscillatorShapes.hpp"
#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct ResetCorrection {
    double correction = 0.0;
    int remaining = 0;
    int length = 1;

    void start(double previous, double current, int samples)
    {
        correction = previous - current;
        length = std::max(samples, 1);
        remaining = length;
    }

    double apply(double value)
    {
        if (remaining <= 0)
            return value;
        const double weight =
            static_cast<double>(remaining)
            / static_cast<double>(length);
        --remaining;
        return value + correction * weight;
    }
};

struct SyncRingOsc : public Unit {
    novelosc::PhaseCore phase;
    novelosc::OversamplingDecimator filters[4];
    ResetCorrection correction1;
    ResetCorrection correction2;
    float previous[7] {};
    double lastModulator1 = 0.0;
    double lastModulator2 = 0.0;
    bool wrapPending = false;
    int carrierWave = 0;
    int modWave1 = 0;
    int modWave2 = 0;
    int oversample = 1;
    bool failed = false;
    bool errorReported = false;
};

void SyncRingOsc_next(
    SyncRingOsc* unit, int inNumSamples)
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
        const double ratio1 = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 1, sample, inNumSamples,
                unit->previous[1], 2.f)),
            -64.0, 64.0);
        const double ratio2 = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 2, sample, inNumSamples,
                unit->previous[2], 3.f)),
            -64.0, 64.0);
        const double amount1 = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 3, sample, inNumSamples,
                unit->previous[3], 0.5f)),
            0.0, 1.0);
        const double amount2 = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 4, sample, inNumSamples,
                unit->previous[4])),
            0.0, 1.0);
        const double phase1 =
            novelosc::rampedInput(
                unit, 5, sample, inNumSamples,
                unit->previous[5])
            * novelosc::kInvTwoPi;
        const double phase2 =
            novelosc::rampedInput(
                unit, 6, sample, inNumSamples,
                unit->previous[6])
            * novelosc::kInvTwoPi;

        const double increment = novelosc::clampValue(
            frequency
                / (sampleRate * unit->oversample),
            -8.0, 8.0);
        double filtered[4] {};
        for (int substep = 0; substep < unit->oversample;
             ++substep) {
            const double carrierPhase = unit->phase.read();
            const double modulatorPhase1 =
                novelosc::wrapCycles(
                    carrierPhase * ratio1 + phase1);
            const double modulatorPhase2 =
                novelosc::wrapCycles(
                    carrierPhase * ratio2 + phase2);
            const double carrier =
                novelosc::oscillatorShape(
                    unit->carrierWave, carrierPhase,
                    increment);
            double modulator1 =
                novelosc::oscillatorShape(
                    unit->modWave1, modulatorPhase1,
                    increment * ratio1);
            double modulator2 =
                novelosc::oscillatorShape(
                    unit->modWave2, modulatorPhase2,
                    increment * ratio2);
            if (unit->wrapPending) {
                unit->correction1.start(
                    unit->lastModulator1, modulator1,
                    unit->oversample * 4);
                unit->correction2.start(
                    unit->lastModulator2, modulator2,
                    unit->oversample * 4);
                unit->wrapPending = false;
            }
            modulator1 =
                unit->correction1.apply(modulator1);
            modulator2 =
                unit->correction2.apply(modulator2);
            const double first =
                carrier
                + amount1
                    * (carrier * modulator1 - carrier);
            const double result =
                first
                + amount2
                    * (first * modulator2 - first);
            const double values[4] {
                carrier, modulator1, modulator2, result
            };
            for (int output = 0; output < 4; ++output)
                filtered[output] =
                    unit->filters[output].process(
                        values[output]);
            unit->lastModulator1 = modulator1;
            unit->lastModulator2 = modulator2;
            if (unit->phase.advance(increment))
                unit->wrapPending = true;
        }
        for (int output = 0; output < 4; ++output)
            outputs[output][sample] =
                novelosc::safeOutput(filtered[output]);
    }

    const float defaults[7] {
        0.f, 2.f, 3.f, 0.5f, 0.f, 0.f, 0.f
    };
    for (int input = 0; input < 7; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            defaults[input]);
}

void SyncRingOsc_Ctor(SyncRingOsc* unit)
{
    SETCALC(SyncRingOsc_next);
    unit->phase = {};
    unit->correction1 = {};
    unit->correction2 = {};
    unit->lastModulator1 = 0.0;
    unit->lastModulator2 = 0.0;
    unit->wrapPending = false;
    unit->carrierWave = 0;
    unit->modWave1 = 0;
    unit->modWave2 = 0;
    unit->oversample = 1;
    unit->failed = false;
    unit->errorReported = false;
    const bool wavesValid =
        novelosc::exactInitInt(
            IN0(7), 0, 3, &unit->carrierWave)
        && novelosc::exactInitInt(
            IN0(8), 0, 3, &unit->modWave1)
        && novelosc::exactInitInt(
            IN0(9), 0, 3, &unit->modWave2);
    unit->oversample =
        novelosc::clampOversample(IN0(10));
    if (!wavesValid) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported, "SyncRingOsc",
            "waveform indices must be integers from 0 to 3.");
        return;
    }
    for (auto& filter : unit->filters) {
        filter = {};
        filter.configure(unit->oversample);
    }
    const float defaults[7] {
        0.f, 2.f, 3.f, 0.5f, 0.f, 0.f, 0.f
    };
    for (int input = 0; input < 7; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerSyncRingOsc()
{
    DefineSimpleCantAliasUnit(SyncRingOsc);
}
