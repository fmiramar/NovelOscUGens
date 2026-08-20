#include "Bandlimit.hpp"
#include "DynamicNetwork.hpp"
#include "HopfCore.hpp"
#include "Interpolation.hpp"
#include "MathUtils.hpp"
#include "OscillatorShapes.hpp"
#include "PhaseCore.hpp"
#include "PhaseSegments.hpp"
#include "RippleGenerator.hpp"
#include "ShiftRegister.hpp"
#include "SpectralFrameBank.hpp"
#include "SpectralTransforms.hpp"
#include "SyncEngine.hpp"
#include "WavetableBank.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual, double expected, double tolerance,
    const char* message)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

void testPhaseCore()
{
    novelosc::PhaseCore phase;
    phase.set(0.9);
    require(phase.advance(0.25), "positive phase wrap");
    requireNear(phase.phase, 0.15, 1.0e-12,
                "positive phase value");
    require(phase.advance(-0.4), "negative phase wrap");
    requireNear(phase.phase, 0.75, 1.0e-12,
                "negative phase value");
    phase.flip();
    requireNear(phase.phase, 0.25, 1.0e-12,
                "half-cycle flip");
    phase.reset();
    requireNear(phase.phase, 0.0, 0.0, "phase reset");
    requireNear(
        phase.read(-0.25), 0.75, 1.0e-12,
        "negative readout offset");
}

void testInterpolation()
{
    const float table[4] { 0.f, 1.f, 0.f, -1.f };
    requireNear(
        novelosc::readNearest(table, 4, 0.25),
        1.0, 0.0, "nearest table read");
    requireNear(
        novelosc::readLinear(table, 4, 0.125),
        0.5, 1.0e-7, "linear table read");
    requireNear(
        novelosc::readLinear(table, 4, -0.125),
        -0.5, 1.0e-7, "wrapped linear table read");
    require(std::isfinite(
                novelosc::readCubic(table, 4, 0.999)),
            "finite cubic table read");
}

void testGeometricSeries()
{
    const std::complex<double> ratio =
        std::polar(0.83, 0.731);
    std::complex<double> direct { 0.0, 0.0 };
    std::complex<double> power { 1.0, 0.0 };
    for (int index = 0; index < 37; ++index) {
        direct += power;
        power *= ratio;
    }
    const std::complex<double> closed =
        novelosc::finiteGeometricSeries(ratio, 37);
    requireNear(
        closed.real(), direct.real(), 1.0e-12,
        "geometric series real component");
    requireNear(
        closed.imag(), direct.imag(), 1.0e-12,
        "geometric series imaginary component");
    requireNear(
        novelosc::finiteGeometricSeries(
            { 1.0, 0.0 }, 128)
            .real(),
        128.0, 0.0, "singular geometric limit");
    requireNear(
        novelosc::geometricMagnitudeSum(1.0, 128),
        128.0, 0.0, "unity magnitude limit");
}

void testFFT()
{
    constexpr int size = 64;
    std::vector<double> real(size);
    std::vector<double> imaginary(size);
    std::vector<double> original(size);
    for (int index = 0; index < size; ++index) {
        original[index] =
            std::sin(novelosc::kTwoPi * 3.0 * index / size)
            + 0.25
                * std::cos(
                    novelosc::kTwoPi * 7.0 * index / size);
        real[index] = original[index];
    }
    novelosc::fftInPlace(
        real.data(), imaginary.data(), size, false);
    novelosc::fftInPlace(
        real.data(), imaginary.data(), size, true);
    for (int index = 0; index < size; ++index)
        requireNear(
            real[index], original[index], 1.0e-10,
            "FFT round trip");
}

double energy(const std::vector<double>& values)
{
    double result = 0.0;
    for (double value : values)
        result += value * value;
    return result;
}

void testBasisTransforms()
{
    constexpr int size = 64;
    std::vector<double> source(size);
    for (int index = 0; index < size; ++index)
        source[index] =
            std::sin(index * 0.37) + 0.1 * index;
    const double sourceEnergy = energy(source);

    std::vector<double> walsh = source;
    novelosc::inverseWalsh(walsh.data(), size);
    requireNear(
        energy(walsh), sourceEnergy, 1.0e-8,
        "Walsh energy preservation");

    std::vector<double> scratch(size);
    std::vector<double> haar = source;
    novelosc::inverseHaar(
        haar.data(), scratch.data(), size);
    requireNear(
        energy(haar), sourceEnergy, 1.0e-8,
        "Haar energy preservation");

    std::vector<double> daubechies = source;
    novelosc::inverseDaubechies4(
        daubechies.data(), scratch.data(), size);
    for (double value : daubechies)
        require(std::isfinite(value),
                "finite Daubechies output");
    requireNear(
        energy(daubechies), sourceEnergy, 1.0e-8,
        "Daubechies energy preservation");
}

void testBandlimitingHelpers()
{
    for (int index = 0; index < 1000; ++index) {
        const double phase =
            static_cast<double>(index) / 1000.0;
        require(std::isfinite(
                    novelosc::directedSaw(
                        phase, 0.01)),
                "finite polyBLEP saw");
        require(std::isfinite(
                    novelosc::directedPulse(
                        phase, -0.01)),
                "finite reverse polyBLEP pulse");
        require(std::isfinite(
                    novelosc::asymmetricTriangle(
                        phase, 0.37, 0.01)),
                "finite polyBLAMP triangle");
    }
    requireNear(
        novelosc::nyquistFade(100.0, 1000.0),
        1.0, 0.0, "Nyquist passband");
    requireNear(
        novelosc::nyquistFade(1000.0, 1000.0),
        0.0, 0.0, "Nyquist culling");
}

double nonHarmonicSpectralEnergy(
    const std::vector<double>& signal, int fundamentalBin)
{
    const int size = static_cast<int>(signal.size());
    std::vector<double> real = signal;
    std::vector<double> imaginary(size, 0.0);
    novelosc::fftInPlace(
        real.data(), imaginary.data(), size, false);
    std::vector<bool> harmonic(size, false);
    harmonic[0] = true;
    for (int multiple = 1;
         multiple * fundamentalBin <= size / 2;
         ++multiple) {
        const int bin = multiple * fundamentalBin;
        harmonic[bin] = true;
        harmonic[size - bin] = true;
    }
    double result = 0.0;
    for (int bin = 0; bin < size; ++bin) {
        if (!harmonic[bin])
            result +=
                real[bin] * real[bin]
                + imaginary[bin] * imaginary[bin];
    }
    return result;
}

void testPolyBlepAliasReduction()
{
    constexpr int size = 4096;
    constexpr int fundamentalBin = 317;
    const double increment =
        static_cast<double>(fundamentalBin) / size;
    std::vector<double> naive(size);
    std::vector<double> corrected(size);
    double phase = 0.0;
    for (int index = 0; index < size; ++index) {
        naive[index] =
            2.0 * novelosc::wrapCycles(phase) - 1.0;
        corrected[index] =
            novelosc::directedSaw(phase, increment);
        phase = novelosc::wrapCycles(phase + increment);
    }
    const double naiveAlias =
        nonHarmonicSpectralEnergy(
            naive, fundamentalBin);
    const double correctedAlias =
        nonHarmonicSpectralEnergy(
            corrected, fundamentalBin);
    require(
        correctedAlias < naiveAlias * 0.2,
        "polyBLEP must reduce non-harmonic alias energy");
}

void testPartialEquation()
{
    const double spacing = 1.0;
    const double neutralExponent = std::exp2(0.0);
    for (int partial = 0; partial < 16; ++partial) {
        const double ratio = std::pow(
            1.0 + partial * spacing, neutralExponent);
        requireNear(
            ratio, partial + 1.0, 1.0e-12,
            "neutral partial ratio");
    }
    require(novelosc::nyquistFade(
                24000.0, 24000.0)
                == 0.0,
            "partial at Nyquist is removed");
}

void testMatrixAndScaleCoordinates()
{
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            require(
                novelosc::rowMajorIndex(row, column, 4)
                    == row * 4 + column,
                "row-major matrix indexing");
        }
    }

    constexpr int scaleSize = 7;
    const double coordinate = -1.25;
    const double periodIndex =
        std::floor(coordinate / scaleSize);
    const double local =
        coordinate - periodIndex * scaleSize;
    requireNear(periodIndex, -1.0, 0.0,
                "negative scale period");
    requireNear(local, 5.75, 1.0e-12,
                "negative scale coordinate wrapping");
    requireNear(
        novelosc::equalPowerA(0.37)
                * novelosc::equalPowerA(0.37)
            + novelosc::equalPowerB(0.37)
                * novelosc::equalPowerB(0.37),
        1.0, 1.0e-12, "equal-power scale crossfade");
}

void testPhaseSegments()
{
    const double sourceX[2] { 0.25, 0.75 };
    const double sourceY[2] { 0.4, 0.8 };
    double x[2] {};
    double y[2] {};
    novelosc::sanitizePhaseBreakpoints(
        sourceX, sourceY, 2, x, y);
    require(x[0] < x[1], "strict phase breakpoint order");
    requireNear(
        novelosc::mapPhaseSegments(0.125, x, y, 2),
        0.2, 1.0e-12, "first phase segment");
    requireNear(
        novelosc::mapPhaseSegments(0.5, x, y, 2),
        0.6, 1.0e-12, "middle phase segment");
    const double neutralX[1] { 0.5 };
    const double neutralY[1] { 0.5 };
    for (int index = 0; index < 100; ++index) {
        const double phase = index / 100.0;
        requireNear(
            novelosc::mapPhaseSegments(
                phase, neutralX, neutralY, 1),
            phase, 1.0e-12, "neutral vector phase");
    }
    requireNear(
        novelosc::bendUnitInterval(0.0, 1.0),
        0.0, 0.0, "phase bend lower endpoint");
    requireNear(
        novelosc::bendUnitInterval(1.0, -1.0),
        1.0, 0.0, "phase bend upper endpoint");

    const double reversedX[2] { 0.75, 0.25 };
    const double reversedY[2] { 0.8, 0.4 };
    novelosc::sanitizePhaseBreakpoints(
        reversedX, reversedY, 2, x, y);
    requireNear(x[0], 0.25, 1.0e-12,
                "phase points sort by x position");
    requireNear(y[0], 0.4, 1.0e-12,
                "phase sorting preserves x/y pairs");
    requireNear(x[1], 0.75, 1.0e-12,
                "phase points remain strictly ordered");
    requireNear(y[1], 0.8, 1.0e-12,
                "phase pair identity remains intact");
}

void testDynamicNetwork()
{
    constexpr int nodes = 32;
    std::vector<double> displacement(nodes);
    std::vector<double> frozen(nodes);
    std::vector<double> velocity(nodes, 0.0);
    std::vector<double> acceleration(nodes, 0.0);
    for (int node = 0; node < nodes; ++node)
        displacement[node] = std::sin(
            novelosc::kTwoPi * node / nodes);
    frozen = displacement;
    novelosc::advanceNetwork(
        displacement.data(), velocity.data(),
        acceleration.data(), nodes, 1,
        0.4, 0.0, 0.0, 8, 0.0, 0.0);
    require(
        displacement == frozen,
        "zero elapsed time freezes network exactly");
    const double initial = novelosc::networkEnergy(
        displacement.data(), velocity.data(),
        nodes, 1, 0.4, 0.0);

    std::vector<double> conservativeDisplacement = displacement;
    std::vector<double> conservativeVelocity(nodes, 0.0);
    std::vector<double> conservativeAcceleration(nodes, 0.0);
    const double conservativeInitial = novelosc::networkEnergy(
        conservativeDisplacement.data(),
        conservativeVelocity.data(), nodes, 1, 0.4, 0.0);
    for (int step = 0; step < 10000; ++step)
        novelosc::advanceNetwork(
            conservativeDisplacement.data(),
            conservativeVelocity.data(),
            conservativeAcceleration.data(), nodes, 1,
            0.4, 0.0, 0.0, 8, 0.0, 0.0005);
    const double conservativeFinal = novelosc::networkEnergy(
        conservativeDisplacement.data(),
        conservativeVelocity.data(), nodes, 1, 0.4, 0.0);
    require(
        std::abs(conservativeFinal - conservativeInitial)
                / conservativeInitial
            < 2.0e-4,
        "undamped circular network conserves energy");

    for (int step = 0; step < 1000; ++step)
        novelosc::advanceNetwork(
            displacement.data(), velocity.data(),
            acceleration.data(), nodes, 1,
            0.4, 0.0, 0.2, 8, 0.0, 0.001);
    const double damped = novelosc::networkEnergy(
        displacement.data(), velocity.data(),
        nodes, 1, 0.4, 0.0);
    require(damped < initial, "network damping lowers energy");
    for (double value : displacement)
        require(std::isfinite(value), "finite network displacement");

    std::fill(velocity.begin(), velocity.end(), 0.0);
    novelosc::strikeNetwork(
        velocity.data(), nodes, 1, 0.5, 1.0);
    const int strikeCenter = novelosc::networkNodeAt(0.5, nodes);
    requireNear(velocity[strikeCenter], 0.375, 0.0,
                "strike center weight");
    requireNear(
        velocity[novelosc::floorMod(strikeCenter - 1, nodes)],
        0.25, 0.0, "strike adjacent weight");
    requireNear(
        velocity[novelosc::floorMod(strikeCenter + 2, nodes)],
        0.0625, 0.0, "strike outer weight");
}

void testSpectralFrameHelpers()
{
    const float data[12] {
        1.f, 1.f, 2.f, 0.5f, 3.f, 0.25f,
        1.f, 0.5f, 4.f, 0.75f, 5.f, 0.125f
    };
    const auto pair = novelosc::spectralFramePair(0.5, 2);
    require(pair.first == 0 && pair.second == 1,
            "spectral frame neighbors");
    requireNear(
        novelosc::spectralFrameValue(
            data, 3, pair, 1, 0),
        3.0, 1.0e-12, "spectral ratio interpolation");
    requireNear(
        novelosc::stretchedRatio(4.0, 0.5),
        2.0, 1.0e-12, "spectral stretch");
    requireNear(
        novelosc::partialActivation(2.25, 2),
        0.25, 1.0e-12, "fractional partial activation");
}

void testRippleAndRegister()
{
    novelosc::RippleState ripple;
    ripple = {};
    constexpr double interval = 1.0 / 48000.0;
    for (int index = 0; index < 100; ++index)
        require(std::isfinite(novelosc::processRipple(
            ripple, 1000.0, 0.01, 0.0,
            0.02, 1.0, interval, 24000.0)),
            "finite ripple output");
    requireNear(
        ripple.phase,
        novelosc::wrapCycles(100.0 * 1000.0 / 48000.0),
        1.0e-12, "zero-sweep ripple phase");
    require(ripple.age > 0.002, "ripple age integration");

    uint32_t value = 0b1011u;
    require(
        novelosc::shiftRegister(value, 4, 0u) == 0b0110u,
        "shift-register sequence");
    require(
        novelosc::rotateRegister(value, 4) == 0b0111u,
        "shift-register rotation");
    requireNear(
        novelosc::registerDac(0b110100u, 6, 3),
        6.0 / 7.0, 1.0e-12, "oldest-bit DAC weighting");
}

void testHopfAndSync()
{
    novelosc::HopfState state;
    state = {};
    const double feedback[4] {};
    constexpr double interval = 1.0 / 192000.0;
    for (int index = 0; index < 192000; ++index)
        novelosc::advanceHopfRK4(
            state, interval, novelosc::kTwoPi * 110.0,
            1.0, 1.0, feedback, 0.0, 0.0);
    requireNear(
        state.x * state.x + state.y * state.y,
        1.0, 1.0e-7, "regulated Hopf radius");
    requireNear(
        novelosc::shortestCycleDifference(0.9, 0.1),
        -0.2, 1.0e-12, "shortest wrapped difference");
    require(novelosc::risingEdge(1.0, 0.0),
            "positive trigger edge");
    require(!novelosc::risingEdge(1.0, 1.0),
            "held trigger is not an edge");
    novelosc::CausalBlep correction;
    correction = {};
    correction.add(0.75, 8);
    requireNear(
        correction.process(), 0.75, 1.0e-12,
        "causal event correction begins at jump");
    double tail = 0.0;
    for (int index = 0; index < 8; ++index)
        tail = correction.process();
    requireNear(tail, 0.0, 0.0,
                "causal event correction terminates");
}

void testWavetableBank()
{
    constexpr int tableSize = 4;
    float data[8 * tableSize] {};
    for (int table = 0; table < 8; ++table)
        for (int sample = 0; sample < tableSize; ++sample)
            data[table * tableSize + sample] =
                static_cast<float>(table);
    const novelosc::WavetableBankView view {
        data, 2, 2, 2, tableSize, 1, 1
    };
    requireNear(
        novelosc::readWavetable3D(
            view, 0, 0.5, 0.5, 0.5, 0.37),
        3.5, 1.0e-12, "trilinear center interpolation");
    requireNear(
        novelosc::readWavetable2D(
            view, 0, 1.0, 1.0, 0.12),
        3.0, 1.0e-12, "shared bilinear backend");
    requireNear(
        novelosc::wavetableMipPosition(
            48000.0 / tableSize, 48000.0,
            tableSize, 4),
        0.0, 0.0, "neutral mip boundary");
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                requireNear(
                    novelosc::readWavetable3D(
                        view, 0,
                        static_cast<double>(x),
                        static_cast<double>(y),
                        static_cast<double>(z), 0.63),
                    static_cast<double>(z * 4 + y * 2 + x),
                    1.0e-12, "trilinear corner selection");
            }
        }
    }
}

void testFiniteGuards()
{
    int parsed = -1;
    require(
        novelosc::exactIntegerInRange(4.f, 0, 8, &parsed)
            && parsed == 4,
        "exact initialization integer");
    require(
        !novelosc::exactIntegerInRange(4.5f, 0, 8, &parsed),
        "fractional initialization integer rejection");
    require(
        !novelosc::exactIntegerInRange(
            std::numeric_limits<float>::max(), 0, 8, &parsed),
        "huge initialization integer rejection");
    require(
        !novelosc::exactIntegerInRange(
            std::numeric_limits<float>::infinity(), 0, 8,
            &parsed),
        "infinite initialization integer rejection");
    require(
        novelosc::roundedClampedInteger(
            std::numeric_limits<float>::max(), 0, 3, 0)
            == 3,
        "huge rounded control clamps before conversion");
    require(
        novelosc::roundedClampedInteger(
            std::numeric_limits<float>::quiet_NaN(),
            0, 3, 2)
            == 2,
        "non-finite rounded control uses fallback");

    requireNear(
        novelosc::safeOutput(
            std::numeric_limits<double>::quiet_NaN()),
        0.0, 0.0, "NaN output guard");
    requireNear(
        novelosc::safeOutput(
            std::numeric_limits<double>::infinity()),
        0.0, 0.0, "infinite output guard");
    requireNear(
        novelosc::safeOutput(1000.0),
        novelosc::kOutputLimit, 0.0,
        "positive output bound");
    requireNear(
        novelosc::wrapCycles(
            std::numeric_limits<double>::infinity()),
        0.0, 0.0, "non-finite phase guard");

    novelosc::OversamplingDecimator decimator;
    decimator = {};
    decimator.configure(8);
    require(decimator.factor == 8, "8x decimator configuration");
    for (int index = 0; index < 2048; ++index) {
        const double output = decimator.process(
            index == 0 ? 1.0 : 0.0);
        require(std::isfinite(output),
                "finite decimator impulse response");
    }
}

} // namespace

int main()
{
    try {
        testPhaseCore();
        testInterpolation();
        testGeometricSeries();
        testFFT();
        testBasisTransforms();
        testBandlimitingHelpers();
        testPolyBlepAliasReduction();
        testPartialEquation();
        testMatrixAndScaleCoordinates();
        testPhaseSegments();
        testDynamicNetwork();
        testSpectralFrameHelpers();
        testRippleAndRegister();
        testHopfAndSync();
        testWavetableBank();
        testFiniteGuards();
        std::cout
            << "NovelOscUGens reference tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "NovelOscUGens reference test failure: "
            << error.what() << '\n';
        return 1;
    }
}
