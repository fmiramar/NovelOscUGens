// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <algorithm>
#include <cstdint>

namespace novelosc {

struct XorShift32 {
    uint32_t state = 0x6d2b79f5u;

    void seed(uint32_t value)
    {
        state = value != 0u ? value : 0x6d2b79f5u;
    }

    uint32_t next()
    {
        uint32_t value = state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state = value != 0u ? value : 0x6d2b79f5u;
        return state;
    }

    double uniform()
    {
        return static_cast<double>(next())
            / 4294967296.0;
    }
};

inline uint32_t registerMask(int bits)
{
    bits = clampValue(bits, 1, 32);
    return bits == 32
        ? 0xffffffffu
        : ((uint32_t { 1 } << bits) - 1u);
}

inline uint32_t oldestRegisterBit(
    uint32_t value, int bits)
{
    return (value >> (bits - 1)) & 1u;
}

inline uint32_t shiftRegister(
    uint32_t value, int bits, uint32_t input)
{
    return ((value << 1) | (input & 1u))
        & registerMask(bits);
}

inline uint32_t rotateRegister(uint32_t value, int bits)
{
    return shiftRegister(
        value, bits, oldestRegisterBit(value, bits));
}

inline double registerDac(
    uint32_t value, int bits, int dacBits)
{
    bits = clampValue(bits, 1, 32);
    dacBits = clampValue(dacBits, 1, bits);
    uint32_t converted = 0u;
    for (int index = 0; index < dacBits; ++index) {
        const int source = bits - 1 - index;
        converted =
            (converted << 1) | ((value >> source) & 1u);
    }
    const uint64_t maximum =
        (uint64_t { 1 } << dacBits) - 1u;
    return maximum > 0
        ? static_cast<double>(converted)
            / static_cast<double>(maximum)
        : 0.0;
}

} // namespace novelosc
