// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <cmath>

namespace novelosc {

struct HopfState {
    double x = 1.0;
    double y = 0.0;
};

struct HopfDerivative {
    double x = 0.0;
    double y = 0.0;
};

inline HopfDerivative hopfDerivative(
    const HopfState& state, double omega,
    double stability, double drive,
    const double* feedback, double injection,
    double input)
{
    const double radiusSquared =
        state.x * state.x + state.y * state.y;
    const double scale =
        std::max(std::abs(omega), kTwoPi);
    const double regulation =
        clampValue(stability, 0.0, 16.0)
        * scale * (1.0 - radiusSquared);
    drive = clampValue(drive, 0.0, 16.0);
    const double forceX = scale * std::tanh(
        drive * (
            feedback[0] * state.x
            - feedback[2] * state.x
            + injection * input));
    const double forceY = scale * std::tanh(
        drive * (
            feedback[1] * state.y
            - feedback[3] * state.y));
    return {
        omega * state.y + regulation * state.x + forceX,
        -omega * state.x + regulation * state.y + forceY
    };
}

inline HopfState hopfAdd(
    const HopfState& state, const HopfDerivative& derivative,
    double amount)
{
    return {
        state.x + derivative.x * amount,
        state.y + derivative.y * amount
    };
}

inline void advanceHopfRK4(
    HopfState& state, double interval, double omega,
    double stability, double drive,
    const double* feedback, double injection,
    double input)
{
    const HopfDerivative a = hopfDerivative(
        state, omega, stability, drive,
        feedback, injection, input);
    const HopfDerivative b = hopfDerivative(
        hopfAdd(state, a, interval * 0.5),
        omega, stability, drive,
        feedback, injection, input);
    const HopfDerivative c = hopfDerivative(
        hopfAdd(state, b, interval * 0.5),
        omega, stability, drive,
        feedback, injection, input);
    const HopfDerivative d = hopfDerivative(
        hopfAdd(state, c, interval),
        omega, stability, drive,
        feedback, injection, input);
    state.x += interval
        * (a.x + 2.0 * b.x + 2.0 * c.x + d.x)
        / 6.0;
    state.y += interval
        * (a.y + 2.0 * b.y + 2.0 * c.y + d.y)
        / 6.0;

    if (!std::isfinite(state.x)
        || !std::isfinite(state.y)) {
        state = {};
        return;
    }
    const double radiusSquared =
        state.x * state.x + state.y * state.y;
    if (radiusSquared > 16.0) {
        const double scale = 4.0 / std::sqrt(radiusSquared);
        state.x *= scale;
        state.y *= scale;
    }
    state.x = flushDenormal(state.x);
    state.y = flushDenormal(state.y);
}

} // namespace novelosc
