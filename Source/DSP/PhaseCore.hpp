// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

namespace novelosc {

struct PhaseCore {
    double phase = 0.0;

    void reset() { phase = 0.0; }

    void flip() { phase = wrapCycles(phase + 0.5); }

    void set(double cycles) { phase = wrapCycles(cycles); }

    double read(double offsetCycles = 0.0) const
    {
        return wrapCycles(phase + offsetCycles);
    }

    bool advance(double increment)
    {
        increment = finiteOr(increment, 0.0);
        increment = clampValue(increment, -16.0, 16.0);
        const double next = phase + increment;
        const bool wrapped = next >= 1.0 || next < 0.0;
        phase = wrapCycles(next);
        return wrapped;
    }
};

} // namespace novelosc
