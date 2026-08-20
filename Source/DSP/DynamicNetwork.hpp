// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MathUtils.hpp"

#include <algorithm>
#include <cmath>

namespace novelosc {

inline int networkNodeAt(double position, int nodes)
{
    position = clampValue(finiteOr(position, 0.5), 0.0, 1.0);
    return clampValue(
        static_cast<int>(std::lround(position * (nodes - 1))),
        0, nodes - 1);
}

inline double networkStableStep(
    double stiffness, double centering)
{
    const double maximumFrequency = std::sqrt(
        std::max(4.0 * stiffness + centering, 1.0e-12));
    return 0.35 / maximumFrequency;
}

inline void networkAcceleration(
    const double* displacement, const double* velocity,
    double* acceleration, int nodes, int topology,
    double stiffness, double centering, double damping,
    int excitationNode, double excitation)
{
    stiffness = clampValue(stiffness, 0.0, 1.0);
    centering = clampValue(centering, 0.0, 1.0);
    damping = clampValue(damping, 0.0, 2.0);
    for (int node = 0; node < nodes; ++node) {
        if (topology == 0
            && (node == 0 || node == nodes - 1)) {
            acceleration[node] = 0.0;
            continue;
        }
        const int left = topology == 1
            ? floorMod(node - 1, nodes)
            : std::max(0, node - 1);
        const int right = topology == 1
            ? floorMod(node + 1, nodes)
            : std::min(nodes - 1, node + 1);
        const double laplacian =
            displacement[left] - 2.0 * displacement[node]
            + displacement[right];
        double force =
            stiffness * laplacian
            - centering * displacement[node]
            - damping * velocity[node];
        const int distance = std::abs(node - excitationNode);
        if (distance == 0)
            force += excitation * 0.5;
        else if (distance == 1
                 || (topology == 1
                     && distance == nodes - 1))
            force += excitation * 0.25;
        acceleration[node] = force;
    }
}

inline void advanceNetwork(
    double* displacement, double* velocity,
    double* acceleration, int nodes, int topology,
    double stiffness, double centering, double damping,
    int excitationNode, double excitation,
    double elapsed)
{
    if (!(elapsed > 0.0) || nodes < 2)
        return;
    const double limit =
        networkStableStep(stiffness, centering);
    const int substeps = clampValue(
        static_cast<int>(std::ceil(elapsed / limit)),
        1, 64);
    const double step = elapsed / substeps;
    for (int substep = 0; substep < substeps; ++substep) {
        networkAcceleration(
            displacement, velocity, acceleration, nodes,
            topology, stiffness, centering, damping,
            excitationNode, excitation);
        for (int node = 0; node < nodes; ++node)
            velocity[node] += 0.5 * step * acceleration[node];
        for (int node = 0; node < nodes; ++node)
            displacement[node] += step * velocity[node];
        if (topology == 0) {
            displacement[0] = 0.0;
            displacement[nodes - 1] = 0.0;
            velocity[0] = 0.0;
            velocity[nodes - 1] = 0.0;
        }
        networkAcceleration(
            displacement, velocity, acceleration, nodes,
            topology, stiffness, centering, damping,
            excitationNode, excitation);
        for (int node = 0; node < nodes; ++node) {
            velocity[node] += 0.5 * step * acceleration[node];
            displacement[node] =
                clampValue(displacement[node], -8.0, 8.0);
            velocity[node] =
                clampValue(velocity[node], -32.0, 32.0);
        }
    }
}

inline void strikeNetwork(
    double* velocity, int nodes, int topology,
    double position, double amplitude)
{
    const int center = networkNodeAt(position, nodes);
    constexpr double weights[5] {
        0.0625, 0.25, 0.375, 0.25, 0.0625
    };
    for (int offset = -2; offset <= 2; ++offset) {
        int node = center + offset;
        if (topology == 1)
            node = floorMod(node, nodes);
        else if (node < 0 || node >= nodes
                 || node == 0 || node == nodes - 1)
            continue;
        velocity[node] +=
            clampValue(amplitude, -8.0, 8.0)
            * weights[offset + 2];
    }
}

inline double networkEnergy(
    const double* displacement, const double* velocity,
    int nodes, int topology, double stiffness,
    double centering)
{
    double energy = 0.0;
    for (int node = 0; node < nodes; ++node) {
        energy += 0.5 * velocity[node] * velocity[node]
            + 0.5 * centering
                * displacement[node] * displacement[node];
        if (node + 1 < nodes) {
            const double difference =
                displacement[node + 1] - displacement[node];
            energy += 0.5 * stiffness
                * difference * difference;
        }
    }
    if (topology == 1) {
        const double difference =
            displacement[0] - displacement[nodes - 1];
        energy += 0.5 * stiffness
            * difference * difference;
    }
    return energy / std::max(nodes, 1);
}

} // namespace novelosc
