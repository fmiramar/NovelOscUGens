// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SC_PlugIn.h"

#include <cstdint>

namespace novelosc {

inline SndBuf* resolveBuffer(Unit* unit, int bufferNumber)
{
    if (bufferNumber < 0)
        return nullptr;
    World* world = unit->mWorld;
    if (static_cast<uint32_t>(bufferNumber)
        < world->mNumSndBufs)
        return world->mSndBufs + bufferNumber;

    const int localNumber =
        bufferNumber - static_cast<int>(world->mNumSndBufs);
    Graph* parent = unit->mParent;
    if (!parent || localNumber < 0
        || localNumber > parent->localMaxBufNum)
        return nullptr;
    return parent->mLocalSndBufs + localNumber;
}

inline bool bufferIdentityMatches(
    Unit* unit, int bufferNumber, SndBuf* expected,
    const float* expectedData, int expectedSamples)
{
    SndBuf* buffer = resolveBuffer(unit, bufferNumber);
    return buffer && buffer == expected && buffer->data
        && buffer->data == expectedData
        && buffer->channels == 1
        && static_cast<int>(buffer->samples)
            == expectedSamples;
}

} // namespace novelosc
