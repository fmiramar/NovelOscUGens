// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/Bandlimit.hpp"
#include "DSP/OscillatorShapes.hpp"
#include "DSP/PhaseCore.hpp"
#include "DSP/SyncEngine.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct SyncModeOsc : public Unit {
    novelosc::PhaseCore phase;
    novelosc::OversamplingDecimator filter;
    novelosc::CausalBlep eventCorrection;
    double direction = 1.0;
    double pullRemaining = 0.0;
    int pullSamples = 0;
    float previous[5] {};
    float previousSync = 0.f;
    int mode = 0;
    int shape = 0;
    int oversample = 2;
    bool armed = false;
    bool failed = false;
};

double waveform(
    const SyncModeOsc* unit, double phase,
    double symmetry, double increment)
{
    if (unit->shape == 1)
        return novelosc::asymmetricTriangle(
            phase, symmetry, increment);
    if (unit->shape == 2)
        return novelosc::directedSaw(phase, increment);
    if (unit->shape == 3)
        return novelosc::directedPulse(
            phase, increment, symmetry);
    return std::sin(novelosc::kTwoPi * phase);
}

void addPhaseJumpCorrection(
    SyncModeOsc* unit, double oldPhase, double newPhase,
    double symmetry, double increment)
{
    const double before = waveform(
        unit, oldPhase, symmetry, increment);
    const double after = waveform(
        unit, newPhase, symmetry, increment);
    unit->eventCorrection.add(before - after, 16);
}

void SyncModeOsc_next(
    SyncModeOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    float* output = OUT(0);
    const float defaults[5] {
        110.f, 0.f, 0.f, 0.5f, 0.02f
    };

    for (int sample = 0; sample < inNumSamples; ++sample) {
        const double frequency = novelosc::rampedInput(
            unit, 0, sample, inNumSamples,
            unit->previous[0], defaults[0]);
        const float sync = static_cast<float>(
            novelosc::inputSample(unit, 1, sample, 0.f));
        const double resetPhase = novelosc::wrapCycles(
            novelosc::rampedInput(
                unit, 3, sample, inNumSamples,
                unit->previous[2], defaults[2]));
        const double symmetry = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 4, sample, inNumSamples,
                unit->previous[3], defaults[3])),
            0.02, 0.98);
        const double softness = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 6, sample, inNumSamples,
                unit->previous[4], defaults[4])),
            0.0, 10.0);
        const double increment =
            novelosc::clampValue(
                frequency * unit->direction
                    / (sampleRate * unit->oversample),
                -8.0, 8.0);
        const bool edge =
            novelosc::risingEdge(sync, unit->previousSync);
        unit->previousSync = sync;

        if (edge) {
            const double oldPhase = unit->phase.read();
            if (unit->mode == 0)
                unit->phase.set(resetPhase);
            else if (unit->mode == 1)
                unit->direction = -unit->direction;
            else if (unit->mode == 2)
                unit->phase.flip();
            else if (unit->mode == 3) {
                unit->pullRemaining =
                    novelosc::shortestCycleDifference(
                        resetPhase, unit->phase.read());
                unit->pullSamples = std::max(
                    1, static_cast<int>(
                        std::lround(softness * sampleRate)));
            } else if (unit->mode == 4)
                unit->armed = true;
            if (unit->mode == 0 || unit->mode == 2)
                addPhaseJumpCorrection(
                    unit, oldPhase, unit->phase.read(),
                    symmetry, increment);
        }

        if (unit->pullSamples > 0) {
            const double correction =
                unit->pullRemaining / unit->pullSamples;
            unit->phase.set(unit->phase.read() + correction);
            unit->pullRemaining -= correction;
            --unit->pullSamples;
            if (unit->pullSamples == 0)
                unit->pullRemaining = 0.0;
        }

        double value = 0.0;
        const bool hold = unit->mode == 5 && sync > 0.f;
        for (int substep = 0;
             substep < unit->oversample; ++substep) {
            value = unit->filter.process(
                waveform(
                    unit, unit->phase.read(),
                    symmetry, increment));
            if (!hold && unit->phase.advance(increment)
                && unit->armed) {
                const double oldPhase = unit->phase.read();
                unit->phase.set(resetPhase);
                unit->armed = false;
                addPhaseJumpCorrection(
                    unit, oldPhase, unit->phase.read(),
                    symmetry, increment);
            }
        }
        output[sample] = novelosc::safeOutput(
            value + unit->eventCorrection.process());
    }
    novelosc::finishRampedInput(
        unit, 0, unit->previous[0], defaults[0]);
    novelosc::finishRampedInput(
        unit, 3, unit->previous[2], defaults[2]);
    novelosc::finishRampedInput(
        unit, 4, unit->previous[3], defaults[3]);
    novelosc::finishRampedInput(
        unit, 6, unit->previous[4], defaults[4]);
}

void SyncModeOsc_Ctor(SyncModeOsc* unit)
{
    SETCALC(SyncModeOsc_next);
    unit->phase = {};
    unit->filter = {};
    unit->eventCorrection = {};
    unit->direction = 1.0;
    unit->pullRemaining = 0.0;
    unit->pullSamples = 0;
    std::fill(unit->previous, unit->previous + 5, 0.f);
    unit->previousSync = novelosc::finiteOr(IN0(1), 0.f);
    unit->mode = 0;
    unit->shape = 0;
    unit->oversample = 2;
    unit->armed = false;
    unit->failed = false;
    const bool valid =
        novelosc::exactInitInt(IN0(2), 0, 5, &unit->mode)
        && novelosc::exactInitInt(IN0(5), 0, 3, &unit->shape)
        && novelosc::exactInitInt(IN0(7), 1, 4, &unit->oversample)
        && (unit->oversample == 1
            || unit->oversample == 2
            || unit->oversample == 4);
    if (!valid) {
        unit->failed = true;
        Print("SyncModeOsc: invalid mode, shape, or oversampling factor.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    unit->filter.configure(unit->oversample);
    const float defaults[5] {
        110.f, 0.f, 0.f, 0.5f, 0.02f
    };
    unit->previous[0] = novelosc::finiteOr(IN0(0), defaults[0]);
    unit->previous[1] = novelosc::finiteOr(IN0(1), defaults[1]);
    unit->previous[2] = novelosc::finiteOr(IN0(3), defaults[2]);
    unit->previous[3] = novelosc::finiteOr(IN0(4), defaults[3]);
    unit->previous[4] = novelosc::finiteOr(IN0(6), defaults[4]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerSyncModeOsc()
{
    DefineSimpleCantAliasUnit(SyncModeOsc);
}
