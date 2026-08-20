# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory = $true)]
    [string]$SCSource,
    [string]$BuildDir = "build-windows",
    [string]$InstallPrefix = "build-windows/stage",
    [ValidateSet("ON", "OFF")]
    [string]$Supernova = "OFF"
)

$ErrorActionPreference = "Stop"

cmake -S . -B $BuildDir -A x64 `
    "-DSC_PATH=$SCSource" `
    -DSCSYNTH=ON `
    "-DSUPERNOVA=$Supernova" `
    -DSTRICT=ON `
    -DBUILD_TESTING=ON
cmake --build $BuildDir --config Release --parallel
ctest --test-dir $BuildDir -C Release --output-on-failure
cmake --install $BuildDir --config Release --prefix $InstallPrefix
