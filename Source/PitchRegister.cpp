// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/SyncEngine.hpp"

#include <algorithm>

namespace {

struct BiroPitchRegister : public Unit {
    double* degrees = nullptr;
    float previousTrigger = 0.f;
    int count = 0;
    bool failed = false;
};

void BiroPitchRegister_next(
    BiroPitchRegister* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    for (int sample = 0; sample < inNumSamples; ++sample) {
        const float trigger =
            novelosc::inputSample(unit, 0, sample, 0.f);
        if (novelosc::risingEdge(
                trigger, unit->previousTrigger)) {
            for (int index = unit->count - 1;
                 index > 0; --index)
                unit->degrees[index] =
                    unit->degrees[index - 1];
            unit->degrees[0] =
                novelosc::inputSample(
                    unit, 1, sample, 0.f);
        }
        unit->previousTrigger = trigger;
        for (int output = 0; output < unit->count; ++output)
            OUT(output)[sample] =
                novelosc::safeOutput(unit->degrees[output]);
    }
}

void BiroPitchRegister_Ctor(BiroPitchRegister* unit)
{
    SETCALC(BiroPitchRegister_next);
    unit->degrees = nullptr;
    unit->previousTrigger =
        novelosc::finiteOr(IN0(0), 0.f);
    unit->count = 0;
    unit->failed = false;
    const bool valid =
        novelosc::exactInitInt(IN0(2), 1, 32, &unit->count)
        && static_cast<int>(unit->mNumOutputs) == unit->count
        && static_cast<int>(unit->mNumInputs)
            == 3 + unit->count;
    if (!valid) {
        unit->failed = true;
        Print("BiroPitchRegister: invalid register layout.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    unit->degrees =
        novelosc::allocateRT<double>(unit->mWorld, unit->count);
    if (!unit->degrees) {
        unit->failed = true;
        Print("BiroPitchRegister: real-time allocation failed.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }
    for (int index = 0; index < unit->count; ++index)
        unit->degrees[index] =
            novelosc::finiteOr(IN0(index + 3), 0.f);
    ClearUnitOutputs(unit, 1);
}

void BiroPitchRegister_Dtor(BiroPitchRegister* unit)
{
    novelosc::releaseRT(unit->mWorld, unit->degrees);
}

} // namespace

void registerBiroPitchRegister()
{
    DefineDtorCantAliasUnit(BiroPitchRegister);
}
