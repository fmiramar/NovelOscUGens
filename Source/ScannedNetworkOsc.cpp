// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/DynamicNetwork.hpp"
#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

struct ScannedNetworkOsc : public Unit {
    novelosc::PhaseCore scanPhase;
    double* displacement = nullptr;
    double* velocity = nullptr;
    double* acceleration = nullptr;
    double physicalClock = 0.0;
    double rawEnergy = 0.0;
    double smoothedEnergy = 0.0;
    float previous[9] {};
    float previousStrike = 0.f;
    int nodes = 0;
    int scanPath = 0;
    int topology = 1;
    bool alternateReverse = false;
    bool failed = false;
};

double cubicSample(
    const ScannedNetworkOsc* unit, double coordinate)
{
    const bool circular = unit->topology == 1;
    const double position = circular
        ? novelosc::wrapCycles(coordinate) * unit->nodes
        : novelosc::clampValue(coordinate, 0.0, 1.0)
            * (unit->nodes - 1);
    const int base = static_cast<int>(std::floor(position));
    const double fraction = position - base;
    const auto nodeValue = [&](int index) {
        if (circular)
            index = novelosc::floorMod(index, unit->nodes);
        else
            index = novelosc::clampValue(
                index, 0, unit->nodes - 1);
        return unit->displacement[index];
    };
    const double y0 = nodeValue(base - 1);
    const double y1 = nodeValue(base);
    const double y2 = nodeValue(base + 1);
    const double y3 = nodeValue(base + 2);
    const double a0 =
        -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
    const double a1 =
        y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double a2 = -0.5 * y0 + 0.5 * y2;
    return ((a0 * fraction + a1) * fraction + a2)
        * fraction + y1;
}

double scanCoordinate(const ScannedNetworkOsc* unit)
{
    const double phase = unit->scanPhase.read();
    if (unit->scanPath == 1)
        return unit->alternateReverse ? 1.0 - phase : phase;
    if (unit->scanPath == 2)
        return phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0;
    return phase;
}

void ScannedNetworkOsc_next(
    ScannedNetworkOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const double sampleRate = unit->mRate->mSampleRate;
    const double physicalInterval = sampleRate / 1000.0;
    const double energyCoefficient =
        1.0 - std::exp(-1.0 / (0.02 * sampleRate));
    float* signalOutput = OUT(0);
    float* energyOutput = OUT(1);

    for (int sample = 0; sample < inNumSamples; ++sample) {
        double controls[9] {};
        const float defaults[9] {
            110.f, 0.2f, 0.25f, 0.01f, 0.02f,
            0.f, 0.f, 1.f, 0.5f
        };
        for (int input = 0; input < 9; ++input)
            controls[input] = novelosc::rampedInput(
                unit, input, sample, inNumSamples,
                unit->previous[input], defaults[input]);

        const double frequency = novelosc::clampValue(
            controls[0], -sampleRate * 8.0, sampleRate * 8.0);
        const double rate =
            novelosc::clampValue(controls[1], 0.0, 20.0);
        const double stiffness =
            novelosc::clampValue(controls[2], 0.0, 1.0);
        const double centering =
            novelosc::clampValue(controls[3], 0.0, 1.0);
        const double damping =
            novelosc::clampValue(controls[4], 0.0, 2.0);
        const double excite =
            novelosc::clampValue(controls[5], -8.0, 8.0);
        const float strike = static_cast<float>(controls[6]);
        const double strikeAmp =
            novelosc::clampValue(controls[7], -8.0, 8.0);
        const double position =
            novelosc::clampValue(controls[8], 0.0, 1.0);
        const bool strikeEdge =
            strike > 0.f && unit->previousStrike <= 0.f;
        unit->previousStrike = strike;

        if (rate > 0.0) {
            if (strikeEdge)
                novelosc::strikeNetwork(
                    unit->velocity, unit->nodes,
                    unit->topology, position, strikeAmp);
            unit->physicalClock += 1.0;
            while (unit->physicalClock >= physicalInterval) {
                unit->physicalClock -= physicalInterval;
                novelosc::advanceNetwork(
                    unit->displacement, unit->velocity,
                    unit->acceleration, unit->nodes,
                    unit->topology, stiffness, centering,
                    damping,
                    novelosc::networkNodeAt(position, unit->nodes),
                    excite, rate * 0.01);
                unit->rawEnergy = 1.0 - std::exp(
                    -novelosc::networkEnergy(
                        unit->displacement, unit->velocity,
                        unit->nodes, unit->topology,
                        stiffness, centering));
            }
        }

        signalOutput[sample] = novelosc::safeOutput(
            cubicSample(unit, scanCoordinate(unit)));
        unit->smoothedEnergy += energyCoefficient
            * (unit->rawEnergy - unit->smoothedEnergy);
        energyOutput[sample] =
            novelosc::safeOutput(unit->smoothedEnergy);

        if (unit->scanPhase.advance(frequency / sampleRate)
            && unit->scanPath == 1)
            unit->alternateReverse = !unit->alternateReverse;
    }

    const float defaults[9] {
        110.f, 0.2f, 0.25f, 0.01f, 0.02f,
        0.f, 0.f, 1.f, 0.5f
    };
    for (int input = 0; input < 9; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            defaults[input]);
}

void ScannedNetworkOsc_Ctor(ScannedNetworkOsc* unit)
{
    SETCALC(ScannedNetworkOsc_next);
    unit->scanPhase = {};
    unit->displacement = nullptr;
    unit->velocity = nullptr;
    unit->acceleration = nullptr;
    unit->physicalClock = 0.0;
    unit->rawEnergy = 0.0;
    unit->smoothedEnergy = 0.0;
    std::fill(std::begin(unit->previous), std::end(unit->previous), 0.f);
    unit->previousStrike = novelosc::finiteOr(IN0(6), 0.f);
    unit->nodes = 0;
    unit->scanPath = 0;
    unit->topology = 1;
    unit->alternateReverse = false;
    unit->failed = false;

    int initShape = 0;
    int seed = 0;
    const bool valid =
        novelosc::exactInitInt(IN0(9), 0, 2, &unit->scanPath)
        && novelosc::exactInitInt(IN0(10), 16, 512, &unit->nodes)
        && novelosc::exactInitInt(IN0(11), 0, 1, &unit->topology)
        && novelosc::exactInitInt(IN0(12), 0, 3, &initShape)
        && novelosc::exactInitInt(
            IN0(13), 0, novelosc::kMaxExactFloatInteger - 1,
            &seed);
    if (!valid) {
        unit->failed = true;
        Print("ScannedNetworkOsc: invalid initialization argument.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }

    unit->displacement =
        novelosc::allocateRT<double>(unit->mWorld, unit->nodes);
    unit->velocity =
        novelosc::allocateRT<double>(unit->mWorld, unit->nodes);
    unit->acceleration =
        novelosc::allocateRT<double>(unit->mWorld, unit->nodes);
    if (!unit->displacement || !unit->velocity
        || !unit->acceleration) {
        unit->failed = true;
        Print("ScannedNetworkOsc: real-time allocation failed.\n");
        ClearUnitOutputs(unit, 1);
        return;
    }

    uint32_t randomState =
        static_cast<uint32_t>(seed != 0 ? seed : 0x7f4a7c15u);
    double previousNoise = 0.0;
    for (int node = 0; node < unit->nodes; ++node) {
        const double position =
            static_cast<double>(node) / (unit->nodes - 1);
        if (initShape == 0)
            unit->displacement[node] =
                std::sin(novelosc::kTwoPi * position);
        else if (initShape == 1)
            unit->displacement[node] =
                1.0 - 4.0 * std::abs(position - 0.5);
        else if (initShape == 2) {
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;
            const double noise =
                2.0 * randomState / 4294967295.0 - 1.0;
            previousNoise = 0.65 * previousNoise + 0.35 * noise;
            unit->displacement[node] = previousNoise;
        }
    }
    if (unit->topology == 0) {
        unit->displacement[0] = 0.0;
        unit->displacement[unit->nodes - 1] = 0.0;
    }
    unit->rawEnergy = 1.0 - std::exp(
        -novelosc::networkEnergy(
            unit->displacement, unit->velocity,
            unit->nodes, unit->topology,
            novelosc::clampValue(
                static_cast<double>(
                    novelosc::finiteOr(IN0(2), 0.25f)),
                0.0, 1.0),
            novelosc::clampValue(
                static_cast<double>(
                    novelosc::finiteOr(IN0(3), 0.01f)),
                0.0, 1.0)));
    const float defaults[9] {
        110.f, 0.2f, 0.25f, 0.01f, 0.02f,
        0.f, 0.f, 1.f, 0.5f
    };
    for (int input = 0; input < 9; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), defaults[input]);
    ClearUnitOutputs(unit, 1);
}

void ScannedNetworkOsc_Dtor(ScannedNetworkOsc* unit)
{
    novelosc::releaseRT(unit->mWorld, unit->displacement);
    novelosc::releaseRT(unit->mWorld, unit->velocity);
    novelosc::releaseRT(unit->mWorld, unit->acceleration);
}

} // namespace

void registerScannedNetworkOsc()
{
    DefineDtorCantAliasUnit(ScannedNetworkOsc);
}
