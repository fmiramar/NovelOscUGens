// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

#include "DSP/PhaseCore.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

namespace {

struct HarmonicStrideOsc : public Unit {
    novelosc::PhaseCore phase;
    float previous[5] {};
    int maxPartials = 1;
    bool failed = false;
    bool errorReported = false;
};

void HarmonicStrideOsc_next(
    HarmonicStrideOsc* unit, int inNumSamples)
{
    if (unit->failed) {
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }

    const double sampleRate = unit->mRate->mSampleRate;
    const double nyquist = sampleRate * 0.5;
    float* sineOutput = OUT(0);
    float* cosineOutput = OUT(1);

    for (int sample = 0; sample < inNumSamples; ++sample) {
        const double frequency = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 0, sample, inNumSamples,
                unit->previous[0])),
            -sampleRate * 8.0, sampleRate * 8.0);
        const double stride = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 1, sample, inNumSamples,
                unit->previous[1], 1.f)),
            1.0e-4, 64.0);
        const double decay = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 2, sample, inNumSamples,
                unit->previous[2], 0.5f)),
            -0.999999, 0.999999);
        const double phaseOffset =
            static_cast<double>(novelosc::rampedInput(
                unit, 3, sample, inNumSamples,
                unit->previous[3]))
            * novelosc::kInvTwoPi;
        const double normalize = novelosc::clampValue(
            static_cast<double>(novelosc::rampedInput(
                unit, 4, sample, inNumSamples,
                unit->previous[4], 1.f)),
            0.0, 1.0);

        const double absoluteFrequency = std::abs(frequency);
        int count = unit->maxPartials;
        if (absoluteFrequency > 1.0e-20) {
            const double maximumRatio =
                nyquist / absoluteFrequency;
            if (maximumRatio <= 1.0)
                count = maximumRatio > 0.0 ? 1 : 0;
            else
                count = std::min(
                    unit->maxPartials,
                    static_cast<int>(std::floor(
                        (maximumRatio - 1.0) / stride))
                        + 1);
        }

        std::complex<double> value { 0.0, 0.0 };
        double amplitudeSum = 0.0;
        if (count > 0) {
            const double basePhase = unit->phase.read();
            const double ratioAngle =
                novelosc::kTwoPi * stride * basePhase
                + (decay < 0.0 ? novelosc::kPi : 0.0);
            const double magnitude = std::abs(decay);
            const std::complex<double> ratio =
                std::polar(magnitude, ratioAngle);
            std::complex<double> series =
                novelosc::finiteGeometricSeries(
                    ratio, count);
            amplitudeSum =
                novelosc::geometricMagnitudeSum(
                    magnitude, count);

            const double lastRatio =
                1.0 + (count - 1) * stride;
            const double lastFade = novelosc::nyquistFade(
                absoluteFrequency * lastRatio,
                nyquist, 0.9);
            if (lastFade < 1.0) {
                const std::complex<double> last =
                    std::pow(ratio, count - 1);
                series -= (1.0 - lastFade) * last;
                amplitudeSum -=
                    (1.0 - lastFade)
                    * std::pow(magnitude, count - 1);
            }

            const std::complex<double> base =
                std::polar(
                    1.0,
                    novelosc::kTwoPi
                        * (basePhase + phaseOffset));
            value = base * series;
        }

        if (normalize > 0.0 && amplitudeSum > 1.0e-20) {
            const double normalizedScale =
                1.0 / amplitudeSum;
            value *= 1.0
                + normalize * (normalizedScale - 1.0);
        }
        sineOutput[sample] =
            novelosc::safeOutput(value.imag());
        cosineOutput[sample] =
            novelosc::safeOutput(value.real());
        unit->phase.advance(frequency / sampleRate);
    }

    for (int input = 0; input < 5; ++input)
        novelosc::finishRampedInput(
            unit, input, unit->previous[input],
            input == 1 ? 1.f : (input == 2 ? 0.5f
                                           : (input == 4 ? 1.f : 0.f)));
}

void HarmonicStrideOsc_Ctor(HarmonicStrideOsc* unit)
{
    SETCALC(HarmonicStrideOsc_next);
    unit->phase = {};
    unit->maxPartials = 1;
    unit->failed = false;
    unit->errorReported = false;
    for (int input = 0; input < 5; ++input)
        unit->previous[input] =
            novelosc::finiteOr(IN0(input), 0.f);
    if (!novelosc::exactInitInt(
            IN0(5), 1, 16384, &unit->maxPartials)) {
        unit->failed = true;
        novelosc::clearAndReport(
            unit, unit->errorReported,
            "HarmonicStrideOsc",
            "maxPartials must be an integer from 1 to 16384.");
        return;
    }
    ClearUnitOutputs(unit, 1);
}

} // namespace

void registerHarmonicStrideOsc()
{
    DefineSimpleCantAliasUnit(HarmonicStrideOsc);
}
