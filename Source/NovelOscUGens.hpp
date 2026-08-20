// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SC_PlugIn.h"
#include "DSP/MathUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

extern InterfaceTable* ft;

namespace novelosc {

template <typename T>
T* allocateRT(World* world, std::size_t count)
{
    if (count == 0
        || count
            > std::numeric_limits<std::size_t>::max()
                / sizeof(T))
        return nullptr;
    T* pointer = static_cast<T*>(
        RTAlloc(world, count * sizeof(T)));
    if (pointer)
        std::fill(pointer, pointer + count, T {});
    return pointer;
}

template <typename T>
void releaseRT(World* world, T*& pointer)
{
    if (pointer) {
        RTFree(world, pointer);
        pointer = nullptr;
    }
}

inline float inputSample(
    Unit* unit, int inputIndex, int sampleIndex,
    float fallback = 0.f)
{
    const float value =
        INRATE(inputIndex) == calc_FullRate
        ? IN(inputIndex)[sampleIndex]
        : IN0(inputIndex);
    return finiteOr(value, fallback);
}

inline float rampedInput(
    Unit* unit, int inputIndex, int sampleIndex,
    int blockSize, float previous, float fallback = 0.f)
{
    if (INRATE(inputIndex) == calc_FullRate)
        return finiteOr(
            IN(inputIndex)[sampleIndex], fallback);
    const float target = finiteOr(IN0(inputIndex), fallback);
    const float fraction = static_cast<float>(
        sampleIndex + 1)
        / static_cast<float>(std::max(blockSize, 1));
    return previous + (target - previous) * fraction;
}

inline void finishRampedInput(
    Unit* unit, int inputIndex, float& previous,
    float fallback = 0.f)
{
    if (INRATE(inputIndex) != calc_FullRate)
        previous = finiteOr(IN0(inputIndex), fallback);
}

inline bool exactInitInt(
    float value, int minimum, int maximum, int* result)
{
    return exactIntegerInRange(
        value, minimum, maximum, result);
}

inline int clampOversample(float value)
{
    if (!std::isfinite(value))
        return 1;
    if (value >= 4.f)
        return 4;
    if (value >= 2.f)
        return 2;
    return 1;
}

inline int clampOversample8(float value)
{
    if (!std::isfinite(value))
        return 1;
    if (value >= 8.f)
        return 8;
    if (value >= 4.f)
        return 4;
    if (value >= 2.f)
        return 2;
    return 1;
}

inline void clearAndReport(
    Unit* unit, bool& errorReported, const char* name,
    const char* message)
{
    if (!errorReported) {
        Print("%s: %s\n", name, message);
        errorReported = true;
    }
    SETCALC(*ClearUnitOutputs);
}

} // namespace novelosc

void registerHarmonicStrideOsc();
void registerTableMorph2D();
void registerTZSplitOsc();
void registerScaleSpreadOsc();
void registerSpectralBasisOsc();
void registerPartialClusterOsc();
void registerSyncRingOsc();
void registerPMNetworkOsc();
void registerClusterPMOsc();
void registerScannedNetworkOsc();
void registerVectorPhaseOsc();
void registerSpectralFrameOsc();
void registerRippleFormantOsc();
void registerShiftLogicOsc();
void registerQuadratureFeedbackOsc();
void registerSyncModeOsc();
void registerTableMorph3D();
void registerBiroPitchRegister();
