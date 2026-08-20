// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/BufferValidation.hpp"
#include "DSP/PhaseCore.hpp"
#include "DSP/WavetableBank.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

struct TableMorph2D : public Unit {
    novelosc::PhaseCore phase;
    float previous[4] {};
    int bufferNumber = -1;
    int tablesX = 0;
    int tablesY = 0;
    int tableSize = 0;
    int mipLevels = 0;
    int interpolation = 0;
    int expectedSamples = 0;
    SndBuf* buffer = nullptr;
    float* data = nullptr;
    bool failed = false;
    bool errorReported = false;
};

novelosc::WavetableBankView bankView(
    const TableMorph2D* unit)
{
    return {
        unit->data, unit->tablesX, unit->tablesY, 1,
        unit->tableSize, unit->mipLevels,
        unit->interpolation
    };
}

void TableMorph2D_next(
    TableMorph2D* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    if (!novelosc::bufferIdentityMatches(
            unit, unit->bufferNumber, unit->buffer,
            unit->data, unit->expectedSamples)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    const double sampleRate = unit->mRate->mSampleRate;
    float* output = OUT(0);
    {
        SndBuf* buffer = unit->buffer;
        LOCK_SNDBUF_SHARED(buffer);
        if (!buffer->data
            || buffer->data != unit->data) {
            unit->failed = true;
            ClearUnitOutputs(unit, inNumSamples);
            return;
        }
        const novelosc::WavetableBankView view =
            bankView(unit);

        for (int sample = 0; sample < inNumSamples;
             ++sample) {
            const double frequency =
                novelosc::rampedInput(
                    unit, 1, sample, inNumSamples,
                    unit->previous[0]);
            const double x = novelosc::rampedInput(
                unit, 2, sample, inNumSamples,
                unit->previous[1]);
            const double y = novelosc::rampedInput(
                unit, 3, sample, inNumSamples,
                unit->previous[2]);
            const double phaseOffset =
                novelosc::rampedInput(
                    unit, 4, sample, inNumSamples,
                    unit->previous[3])
                * novelosc::kInvTwoPi;

            const double mipPosition =
                novelosc::wavetableMipPosition(
                    frequency, sampleRate,
                    unit->tableSize, unit->mipLevels);
            const int mip0 = static_cast<int>(
                std::floor(mipPosition));
            const int mip1 = std::min(
                mip0 + 1, unit->mipLevels - 1);
            const double mipFraction =
                mipPosition - mip0;
            const double readPhase =
                unit->phase.read(phaseOffset);
            const double first =
                novelosc::readWavetable2D(
                    view, mip0, x, y, readPhase);
            const double second =
                novelosc::readWavetable2D(
                    view, mip1, x, y, readPhase);
            output[sample] = novelosc::safeOutput(
                first + (second - first) * mipFraction);
            unit->phase.advance(
                novelosc::clampValue(
                    frequency / sampleRate, -8.0, 8.0));
        }
    }

    const float defaults[4] { 440.f, 0.f, 0.f, 0.f };
    for (int input = 0; input < 4; ++input)
        novelosc::finishRampedInput(
            unit, input + 1, unit->previous[input],
            defaults[input]);
}

void TableMorph2D_Ctor(TableMorph2D* unit)
{
    SETCALC(TableMorph2D_next);
    unit->phase = {};
    unit->bufferNumber = -1;
    unit->tablesX = 0;
    unit->tablesY = 0;
    unit->tableSize = 0;
    unit->mipLevels = 0;
    unit->interpolation = 0;
    unit->expectedSamples = 0;
    unit->buffer = nullptr;
    unit->data = nullptr;
    unit->failed = false;
    unit->errorReported = false;
    bool valid =
        novelosc::exactInitInt(
            IN0(0), 0,
            novelosc::kMaxExactFloatInteger - 1,
            &unit->bufferNumber)
        && novelosc::exactInitInt(
            IN0(5), 1, 1024, &unit->tablesX)
        && novelosc::exactInitInt(
            IN0(6), 1, 1024, &unit->tablesY)
        && novelosc::exactInitInt(
            IN0(7), 64, 65536, &unit->tableSize)
        && novelosc::exactInitInt(
            IN0(8), 1, 16, &unit->mipLevels)
        && novelosc::exactInitInt(
            IN0(9), 0, 2, &unit->interpolation)
        && novelosc::isPowerOfTwo(unit->tableSize);

    const int64_t expected =
        static_cast<int64_t>(unit->tablesX)
        * unit->tablesY * unit->tableSize
        * unit->mipLevels;
    valid = valid && expected > 0
        && expected <= std::numeric_limits<int>::max();
    if (valid)
        unit->expectedSamples =
            static_cast<int>(expected);
    if (valid)
        unit->buffer = novelosc::resolveBuffer(
            unit, unit->bufferNumber);
    valid = valid && unit->buffer
        && unit->buffer->data
        && unit->buffer->channels == 1
        && static_cast<int>(unit->buffer->samples)
            == unit->expectedSamples;
    if (!valid) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported, "TableMorph2D",
            "buffer or declared table layout is invalid.");
        return;
    }
    unit->data = unit->buffer->data;
    const float defaults[4] { 440.f, 0.f, 0.f, 0.f };
    for (int input = 0; input < 4; ++input)
        unit->previous[input] =
            novelosc::finiteOr(
                IN0(input + 1), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerTableMorph2D()
{
    DefineSimpleCantAliasUnit(TableMorph2D);
}
