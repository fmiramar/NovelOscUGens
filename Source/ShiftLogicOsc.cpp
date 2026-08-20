// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/OscillatorShapes.hpp"
#include "DSP/PhaseCore.hpp"
#include "DSP/ShiftRegister.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

struct ShiftLogicOsc : public Unit {
    novelosc::PhaseCore phaseA;
    novelosc::PhaseCore phaseB;
    novelosc::XorShift32 random;
    uint32_t registerValue = 1u;
    double stepped = 0.0;
    double smoothed = 0.0;
    float previous[7] {};
    int bits = 8;
    int dacBits = 3;
    int mode = 1;
    bool failed = false;
};

void ShiftLogicOsc_next(
    ShiftLogicOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    float* oscAOutput = OUT(0);
    float* oscBOutput = OUT(1);
    float* bitOutput = OUT(2);
    float* steppedOutput = OUT(3);
    float* smoothedOutput = OUT(4);
    const float defaults[7] {
        110.f, 137.f, 0.5f, 0.5f, 0.f, 1.f, 0.01f
    };

    for (int sample = 0; sample < inNumSamples; ++sample) {
        double control[7] {};
        for (int input = 0; input < 7; ++input)
            control[input] = novelosc::rampedInput(
                unit, input, sample, inNumSamples,
                unit->previous[input], defaults[input]);
        const double feedbackA =
            novelosc::clampValue(control[2], -8.0, 8.0);
        const double feedbackB =
            novelosc::clampValue(control[3], -8.0, 8.0);
        const double frequencyA = novelosc::clampValue(
            control[0]
                * std::exp2(novelosc::clampValue(
                    feedbackA * unit->stepped, -8.0, 8.0)),
            -sampleRate * 8.0, sampleRate * 8.0);
        const double frequencyB = novelosc::clampValue(
            control[1]
                * std::exp2(novelosc::clampValue(
                    feedbackB * unit->stepped, -8.0, 8.0)),
            -sampleRate * 8.0, sampleRate * 8.0);
        const double incrementA = frequencyA / sampleRate;
        const double incrementB = frequencyB / sampleRate;
        const double oscA =
            novelosc::asymmetricTriangle(
                unit->phaseA.read(), 0.5, incrementA);
        const double oscB =
            novelosc::asymmetricTriangle(
                unit->phaseB.read(), 0.5, incrementB);
        unit->phaseB.advance(incrementB);

        if (unit->phaseA.advance(incrementA)) {
            const uint32_t oldest = novelosc::oldestRegisterBit(
                unit->registerValue, unit->bits);
            uint32_t candidate = oscB > control[4] ? 1u : 0u;
            if (unit->mode == 1)
                candidate ^= oldest;
            else if (unit->mode == 2)
                candidate = oldest;
            else if (unit->mode == 3)
                candidate = unit->random.next() >> 31;
            const double change =
                novelosc::clampValue(control[5], 0.0, 1.0);
            if (unit->random.uniform() < change)
                unit->registerValue = novelosc::shiftRegister(
                    unit->registerValue, unit->bits, candidate);
            else
                unit->registerValue = novelosc::rotateRegister(
                    unit->registerValue, unit->bits);
            unit->stepped =
                2.0 * novelosc::registerDac(
                    unit->registerValue,
                    unit->bits, unit->dacBits)
                - 1.0;
        }
        const double slew =
            novelosc::clampValue(control[6], 0.0, 10.0);
        const double coefficient = slew > 0.0
            ? 1.0 - std::exp(-1.0 / (slew * sampleRate))
            : 1.0;
        unit->smoothed += coefficient
            * (unit->stepped - unit->smoothed);

        oscAOutput[sample] = novelosc::safeOutput(oscA);
        oscBOutput[sample] = novelosc::safeOutput(oscB);
        bitOutput[sample] = static_cast<float>(
            unit->registerValue & 1u);
        steppedOutput[sample] =
            novelosc::safeOutput(unit->stepped);
        smoothedOutput[sample] =
            novelosc::safeOutput(unit->smoothed);
    }
    for (int input = 0; input < 7; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            defaults[input]);
}

void ShiftLogicOsc_Ctor(ShiftLogicOsc* unit)
{
    SETCALC(ShiftLogicOsc_next);
    unit->phaseA = {};
    unit->phaseB = {};
    unit->random = {};
    unit->registerValue = 1u;
    unit->stepped = 0.0;
    unit->smoothed = 0.0;
    std::fill(unit->previous, unit->previous + 7, 0.f);
    unit->bits = 8;
    unit->dacBits = 3;
    unit->mode = 1;
    unit->failed = false;
    int seed = 0;
    const bool valid =
        novelosc::exactInitInt(IN0(7), 3, 32, &unit->bits)
        && novelosc::exactInitInt(
            IN0(8), 1, unit->bits, &unit->dacBits)
        && novelosc::exactInitInt(IN0(9), 0, 3, &unit->mode)
        && novelosc::exactInitInt(
            IN0(10), 0, novelosc::kMaxExactFloatInteger - 1,
            &seed);
    if (!valid) {
        unit->failed = true;
        Print("ShiftLogicOsc: invalid register initialization argument.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    unit->random.seed(static_cast<uint32_t>(seed));
    unit->registerValue =
        unit->random.next() & novelosc::registerMask(unit->bits);
    if (unit->registerValue == 0u)
        unit->registerValue = 1u;
    unit->stepped =
        2.0 * novelosc::registerDac(
            unit->registerValue, unit->bits, unit->dacBits)
        - 1.0;
    unit->smoothed = unit->stepped;
    const float defaults[7] {
        110.f, 137.f, 0.5f, 0.5f, 0.f, 1.f, 0.01f
    };
    for (int input = 0; input < 7; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerShiftLogicOsc()
{
    DefineSimpleCantAliasUnit(ShiftLogicOsc);
}
