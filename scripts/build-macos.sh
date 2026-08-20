#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
    echo "usage: $0 SC_SOURCE [BUILD_DIR] [INSTALL_PREFIX]" >&2
    exit 64
fi

sc_source=$1
build_dir=${2:-build-macos}
install_prefix=${3:-"${build_dir}/stage"}
cmake_args=(
    -S .
    -B "${build_dir}"
    -DSC_PATH="${sc_source}"
    -DCMAKE_BUILD_TYPE=Release
    -DSCSYNTH=ON
    -DSUPERNOVA="${NOVEL_OSC_SUPERNOVA:-OFF}"
    -DSTRICT=ON
    -DBUILD_TESTING=ON
)

if [[ -n "${NOVEL_OSC_ARCHS:-}" ]]; then
    cmake_args+=(
        -DCMAKE_OSX_ARCHITECTURES="${NOVEL_OSC_ARCHS}"
    )
fi

cmake "${cmake_args[@]}"
cmake --build "${build_dir}" --config Release --parallel
ctest --test-dir "${build_dir}" -C Release --output-on-failure
cmake --install "${build_dir}" --config Release \
    --prefix "${install_prefix}"
