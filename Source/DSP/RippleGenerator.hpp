// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <cmath>

namespace novelosc {

struct RippleState {
    double phase = 0.0;
    double age = 0.0;
    bool active = true;

    void reset()
    {
        phase = 0.0;
        age = 0.0;
        active = true;
    }
};

inline double processRipple(
    RippleState& state, double formant, double decay,
    double sweep, double basePeriod, double direction,
    double sampleInterval, double nyquist)
{
    if (!state.active)
        return 0.0;
    formant = clampValue(std::abs(formant), 0.0, nyquist);
    decay = clampValue(decay, sampleInterval, 10.0);
    sweep = clampValue(sweep, -8.0, 8.0);
    const double envelope = std::exp(-state.age / decay);
    if (envelope < 1.0e-7) {
        state.active = false;
        return 0.0;
    }
    const double cyclePosition = basePeriod > sampleInterval
        ? clampValue(state.age / basePeriod, 0.0, 1.0)
        : 1.0;
    const double instantaneous =
        formant * std::exp2(sweep * cyclePosition);
    const double fade =
        nyquistFade(instantaneous, nyquist, 0.85);
    const double output =
        std::sin(kTwoPi * state.phase)
        * envelope * fade;
    state.phase = wrapCycles(
        state.phase
        + direction * instantaneous * sampleInterval);
    state.age += sampleInterval;
    return output;
}

} // namespace novelosc
