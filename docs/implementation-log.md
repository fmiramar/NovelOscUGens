# NovelOscUGens implementation log

Date: 2026-07-30
Target versions: 0.1.0 and 0.2.0
Specifications: internal design records for the original and additional
oscillator sets.

## Scope and source audit

- Read the full 1,684-line implementation specification before coding.
- Classified the project as a standalone experimental implementation: every
  DSP design is original and based on general mathematics or standard
  synthesis techniques rather than a direct upstream code port.
- Searched the installed SuperCollider class library, Extensions, Quarks, and
  available sc3-plugins sources for all nine exact public class names. No
  pre-existing exact-name collision was found.
- Added no compatibility aliases.
- Consulted the official SuperCollider plugin API and current CMake module.
  The older `supercollider/example-plugins` repository is deprecated; current
  official guidance points to
  `supercollider/cookiecutter-supercollider-plugin`.
- Incorporated no third-party DSP source, binary data, firmware, wavetable,
  preset, graphic, or product documentation.

## Project infrastructure

- Created an independent C++17 project and installable `NovelOscUGens`
  extension.
- Added strict-warning `scsynth` and `supernova` targets using
  `SuperColliderServerPlugin.cmake`.
- Added cross-platform release scripts for macOS, Linux, and Windows.
- Added CI jobs for:
  - Linux x86_64;
  - macOS x86_64;
  - macOS arm64;
  - Windows x86_64;
  - Linux supernova;
  - address/undefined-behavior sanitizer reference tests.
- CI artifact names derive their prefix from `github.repository_owner`.
- Added full GPLv3 license text, origins, third-party notices, changelog,
  README, project guide, nine class help pages, tests, and benchmarks.

## Shared DSP

Implemented:

- signed bidirectional phase accumulation and phase-only read offsets;
- finite-value guards, output bounds, denormal flushing, smooth Nyquist fades,
  equal-power crossfades, and exact initialization-integer checks;
- stable finite complex geometric sums, including the unity-ratio limit;
- polynomial BLEP saw/pulse correction and polynomial BLAMP asymmetric
  triangle corners;
- fourth-order oversampling decimation;
- periodic nearest, linear, and cubic interpolation;
- sine, bandlimited triangle, saw, and pulse shape selection;
- global/local buffer resolution, declared-layout validation, identity
  retention, and shared buffer locking;
- in-place radix-2 FFT plus inverse Walsh, Haar, and Daubechies-4 transforms;
- row-major matrix indexing shared by tests and network UGens.

No calculation callback calls `new`, `delete`, `malloc`, `free`, `RTAlloc`, or
`RTFree`. Variable storage is allocated only in the UGen constructor and freed
in its destructor.

## UGen implementations

### HarmonicStrideOsc

- Uses one signed base phase and a stable finite complex geometric sum.
- Supports fractional stride, signed decay, optional absolute-amplitude
  normalization, quadrature outputs, partial caps, and smooth Nyquist culling.

### TableMorph2D

- Validates the exact mono `[mip][y][x][sample]` layout.
- Performs bilinear spatial interpolation, selectable periodic sample
  interpolation, frequency-based mip selection, and adjacent-level crossfade.
- Detects freed or reallocated buffers and becomes silent without logging from
  the audio callback.
- `prepareBuffer` removes DC, optionally peak-normalizes, constructs low-pass
  mips, and loads the flattened buffer.

### TZSplitOsc

- Implements true through-zero motion, audio-rate FM and phase readout
  modulation, sample-accurate reset/flip, BLAMP triangle corners, oversampled
  drive, and four coherent outputs.
- Reset precedes flip when both edges occur on the same sample.
- The even and odd paths reconstruct the full shaped output.

### ScaleSpreadOsc

- Validates an ascending mono ratio buffer beginning at one.
- Maps fixed voices through continuous positive and negative scale
  coordinates, supports non-octave periods, and equal-power crossfades
  neighboring degrees.
- Adds deterministic nearest-neighbor PM and equal-power stereo distribution.
- `prepareScale` validates and loads the ratio collection.

### SpectralBasisOsc

- Allocates two internal banks, transform scratch arrays, and mip levels.
- Generates Fourier, Walsh, Haar, and Daubechies-4 waveforms from one
  deterministic envelope.
- Removes DC, peak-normalizes, refresh-limits block-boundary regeneration,
  builds mips, and crossfades banks over approximately 20 milliseconds.

### PartialClusterOsc

- Implements the documented ratio equation, smooth partial activation,
  spectral tilt, raised-cosine combing, Nyquist fades, deterministic phase
  dispersion, symmetric unison, normalization, and stereo distribution.
- The first implementation evaluated transcendental functions inside every
  partial/voice/sample loop. Benchmarking correctly exposed this as unusable
  for large banks.
- The second implementation selects its strategy at construction:
  - scalar/control-rate shaping block-cooks ratios and weights, then uses
    recursive complex oscillators with continuous phase and block-smoothed
    weights;
  - if any shaping input is audio rate, the original exact per-sample path is
    retained, so audio-rate inputs are not silently downgraded.
- The optimized bank allocates approximately
  `8 * maxPartials + 48 * voices * maxPartials` bytes per instance.

### SyncRingOsc

- Derives two synchronized modulator phases from one signed carrier.
- Supports integer and fractional ratios, phase offsets, four bandlimited
  shapes, two sequential ring crossfades, fractional-reset correction, and
  oversampling.

### PMNetworkOsc

- Implements three operators with row-major 3×3 PM and FM matrices.
- PM affects readout phase; FM changes signed accumulated frequency.
- Feedback reads the previous oversampled substep, defining cyclic networks.

### ClusterPMOsc

- Implements four PM operators, each a normalized four-component sine cluster.
- Uses a row-major 4×4 previous-substep matrix and smoothly removes local
  components at Nyquist.

## Corrections made during verification

- Explicitly initialized every UGen state field in the server constructor.
  SuperCollider owns raw UGen memory, so relying on C++ default member
  initializers would not be robust.
- Corrected asynchronous test synchronization to use a predicate-bearing
  `Condition`, avoiding a callback-before-`hang` race.
- Corrected the NRT script's SuperCollider unary-negation syntax and added a
  test that compiles every test script before execution.
- Corrected `LOCK_SNDBUF_SHARED` calls to use a local identifier. `scsynth`
  defines that macro as a no-op, while supernova token-pastes its argument and
  exposed the portability error.
- Replaced the first `PartialClusterOsc` hot loop with the dual optimized/exact
  strategy described above.

## Verification results

### Strict reference and plugin build

Commands:

```sh
cmake -S . -B build \
  -DSC_PATH=/path/to/supercollider-3.14.1 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DSTRICT=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Result: strict compilation passed with no project warnings; all reference tests
passed. Coverage includes phase wrapping, interpolation, FFT round trips,
Walsh/Haar/Daubechies energy, geometric-series limits, matrix indexing,
negative scale wrapping, partial equations, finite guards, decimator
stability, and measured polyBLEP alias reduction relative to a naive saw.

### Dual scsynth/supernova build and runtime

Configured against a complete SuperCollider 3.14.1 SDK with both
`SCSYNTH=ON` and `SUPERNOVA=ON`.

Result: both `NovelOscUGens.scx` and `NovelOscUGens_supernova.scx` built under
strict warnings; reference tests passed.

The installed supernova binary then booted and executed a finite, non-silent
mix of all nine UGen families, including the shared-buffer lock paths and both
variable-allocation/destructor paths:

```text
NOVEL_OSC_SUPERNOVA_SMOKE_OK peak=0.222
```

### Sanitizers

Configured a non-server Debug build with
`NOVEL_OSC_SANITIZE=ON`, AddressSanitizer, and UndefinedBehaviorSanitizer.

Result: all reference tests passed with no sanitizer report.

### Language and installed-server tests

- All nine classes compiled.
- Every `.ar` method constructed a valid SynthDef.
- All nine `.kr` methods threw as intended.
- Wrong PM/cluster fixed-array sizes were rejected.
- `TableMorph2D.prepareBuffer` produced the declared 768-frame test layout.
- `ScaleSpreadOsc.prepareScale` produced the declared seven-ratio buffer.
- The installed scsynth plugin recorded 25 outputs, including the optimized
  and exact audio-rate `PartialClusterOsc` paths:

```text
NOVEL_OSC_LIVE_SMOKE_OK
peaks=[0.9790, 1.0000, 0.9247, 1.0000, 0.2388, 0.8669,
1.1057, 0.8999, 0.8999, 1.0018, 0.3758, 0.3734, 1.2257,
0.9873, 1.0000, 0.7700, 1.0000, 1.2235, 0.9873, 0.8159,
0.8159, 0.8159, 0.8159, 0.4172, 0.4172]
```

- Every value was finite.
- Every oscillator family was non-silent.
- `TZSplitOsc` even + odd reconstruction error was below `2e-5`.
- `ScaleSpreadOsc` with `stereo=0` produced matching channels within `2e-6`.

### Randomized safety

Rendered 80 deterministic updates containing negative frequency, extreme and
out-of-range controls, rapid matrices, infinity, deliberately missing buffers,
and buffers freed while Synths remained active.

Expected one-shot server messages were produced for the invalid and freed
buffers. The server stayed alive and synchronized:

```text
NOVEL_OSC_RANDOMIZED_SAFETY_OK
```

### NRT matrix

Rendered seven buffer-independent UGen families into 20 channels at sample
rates 44.1, 48, 88.2, 96, and 192 kHz and block sizes 1, 16, 64, and 256.

All 20 renders passed finite/non-silent checks. The 997 Hz reference measured
from `997.042` through `997.136` Hz, TZ reconstruction error was
`5.9604644775391e-08` in every render, and the absolute SpectralBasisOsc mean
stayed from `0.000183` through `0.000681`.

```text
NOVEL_OSC_NRT_METRICS_OK
```

### SCDoc and help examples

- `SCDoc.indexAllDocuments(true)` completed.
- All nine class pages and `Guides/NovelOscillators` parsed and rendered.
- No SCDoc warning or error was emitted for the changed pages.
- All 19 parenthesized runnable example blocks compiled.
- Every class resolves to the installed
  `NovelOscUGens/Classes/NovelOscUGens.sc` file.

## Real-time benchmark

Method:

- release `scsynth`;
- server block size 64;
- requested rates 48 and 96 kHz;
- no-synth baseline sampled first;
- tiny outputs written to an internal audio bus;
- independently scaled batches kept below overload;
- average and peak server CPU sampled after warmup;
- last column is `(average - baseline) / instances`.

The local audio driver block size was 512. Unrelated installed VSTPlugin
libraries printed duplicate Objective-C-class warnings during server startup;
those extensions were not present in the benchmark SynthDefs.

### 48 kHz

Baseline average CPU: `0.027`.

| Case | Instances | Average | Peak | Incremental average / instance |
|---|---:|---:|---:|---:|
| HarmonicStride | 48 | 47.530 | 58.424 | 0.9896 |
| TableMorph2D | 48 | 45.624 | 53.620 | 0.9499 |
| TZSplit 1x | 64 | 28.535 | 34.545 | 0.4454 |
| TZSplit 2x | 48 | 42.233 | 46.204 | 0.8793 |
| TZSplit 4x | 32 | 50.762 | 56.988 | 1.5855 |
| ScaleSpread 8 voices | 16 | 39.635 | 50.614 | 2.4755 |
| ScaleSpread 24 voices | 4 | 30.622 | 43.337 | 7.6486 |
| SpectralBasis size 128 | 24 | 8.984 | 13.895 | 0.3732 |
| SpectralBasis size 2048 | 4 | 6.732 | 21.199 | 1.6762 |
| PartialCluster 64×1 | 8 | 6.914 | 9.896 | 0.8608 |
| PartialCluster 256×5 | 2 | 26.944 | 34.993 | 13.4583 |
| SyncRing 1x | 64 | 73.274 | 91.473 | 1.1445 |
| SyncRing 4x | 16 | 70.642 | 95.035 | 4.4134 |
| PMNetwork 1x static | 48 | 24.943 | 28.331 | 0.5191 |
| PMNetwork 4x static | 16 | 27.835 | 37.643 | 1.7380 |
| PMNetwork 4x audio PM | 16 | 27.895 | 30.149 | 1.7417 |
| ClusterPM 4x | 4 | 18.483 | 26.570 | 4.6139 |

### 96 kHz

Baseline average CPU: `0.039`.

| Case | Instances | Average | Peak | Incremental average / instance |
|---|---:|---:|---:|---:|
| HarmonicStride | 24 | 47.479 | 67.857 | 1.9767 |
| TableMorph2D | 24 | 45.391 | 52.936 | 1.8896 |
| TZSplit 1x | 32 | 28.736 | 34.470 | 0.8968 |
| TZSplit 2x | 24 | 41.113 | 55.690 | 1.7114 |
| TZSplit 4x | 16 | 49.830 | 56.865 | 3.1119 |
| ScaleSpread 8 voices | 8 | 39.677 | 47.011 | 4.9547 |
| ScaleSpread 24 voices | 2 | 30.241 | 37.701 | 15.1011 |
| SpectralBasis size 128 | 12 | 8.772 | 14.634 | 0.7277 |
| SpectralBasis size 2048 | 2 | 4.274 | 19.044 | 2.1176 |
| PartialCluster 64×1 | 4 | 6.846 | 9.368 | 1.7018 |
| PartialCluster 256×5 | 1 | 26.541 | 34.084 | 26.5017 |
| SyncRing 1x | 32 | 71.968 | 82.995 | 2.2478 |
| SyncRing 4x | 8 | 69.605 | 81.351 | 8.6957 |
| PMNetwork 1x static | 24 | 24.614 | 35.824 | 1.0240 |
| PMNetwork 4x static | 8 | 27.715 | 36.765 | 3.4594 |
| PMNetwork 4x audio PM | 8 | 27.895 | 36.176 | 3.4820 |
| ClusterPM 4x | 2 | 18.410 | 22.466 | 9.1854 |

### Benchmark interpretation

- Doubling the server rate roughly doubled per-instance CPU for most
  sample-by-sample paths.
- TZSplit cost was approximately 1.97× at 2x and 3.56× at 4x relative to 1x
  at 48 kHz. SyncRing 4x cost 3.86× its 1x case. PMNetwork 4x cost 3.35× its
  1x case.
- Audio-rate PM routing added almost no cost beyond the 4x PM network itself
  in this patch; the extra modulation oscillator was shared within each graph.
- ScaleSpread cost tracked voice count closely: 24 voices cost about 3.09×
  eight voices at 48 kHz.
- Large SpectralBasis tables mainly increased regeneration work and peak CPU;
  playback itself still reads only adjacent mip levels.
- Before optimization, the same PartialCluster 64×1 and 256×5 cases measured
  approximately `19.7352` and `373.9295` incremental CPU points per instance
  at 48 kHz. The final `0.8608` and `13.4583` readings are about 23× and 28×
  lower.
- A 256×5 PartialCluster is still a heavy single UGen (about 13.46 incremental
  points at 48 kHz and 26.50 at 96 kHz). The benchmark makes this limit
  explicit rather than hiding it behind a smaller default.

These values compare code paths on this run only. They are not portable
percentages for other hardware, audio drivers, thermal states, or server block
sizes.

## Local install

The final dual-server extension is installed in the SuperCollider user
Extensions directory under `NovelOscUGens`. Recompile the class library before
using it in an already-running language process.

## Remaining release-environment work

Local macOS scsynth and supernova runtime, NRT, SCDoc, sanitizer, safety, and
benchmark checks pass. The CI matrix defines Linux, macOS x86_64/arm64, and
Windows x86_64 builds, but those hosted jobs cannot be claimed as passing until
they have run from a published repository checkout.

## v0.2 additional oscillator implementation

### Audit and technical sources

- Read the complete 2,028-line additional-oscillator specification before
  implementation.
- Searched the workspace, installed Extensions, installed Quarks, vanilla
  SuperCollider, and available sc3-plugins sources for all eleven new public
  names. No exact-name collision was found.
- Consulted the Smaragdis scanned-synthesis paper, the Kleimola/Lazzarini/
  Timoney/Välimäki vector-phaseshaping paper, Csound's scanned-synthesis
  reference, and current official SuperCollider oscillator, trigger, delay,
  and buffer source.
- Used those sources for algorithms and semantics only. No upstream DSP source,
  proprietary firmware, circuit data, factory table, preset, or product
  documentation was copied.

### Added implementation

- Added eight public native audio-rate UGens:
  `ScannedNetworkOsc`, `VectorPhaseOsc`, `SpectralFrameOsc`,
  `RippleFormantOsc`, `ShiftLogicOsc`, `QuadratureFeedbackOsc`,
  `SyncModeOsc`, and `TableMorph3D`.
- Added `RatioFamilyOsc` and `PitchRegisterOsc` as language-side oscillator
  graph constructors, plus the `SpectralFrameBuffer` preparation object.
- Added a small internal `BiroPitchRegister` server primitive. A LocalBuf/
  Demand graph cannot guarantee that downstream audio readers observe every
  intermediate register state when multiple triggers occur in one server
  block; only the register was moved native. Scale lookup, glide, and
  oscillator voices remain language-side.
- Added fixed-capacity/shared helpers for dynamic networks, piecewise phase
  segments, spectral-frame layout, ripple integration, shift-register logic,
  Hopf RK4 integration, causal sync correction, and 2D/3D wavetable access.
- Refactored TableMorph2D to the shared wavetable backend and removed all
  freed-buffer logging from its real-time callback.
- Extended the decimator to 8x for QuadratureFeedbackOsc.
- Added final `mul` and `add` to every original and new public `.ar` wrapper.
  The original nine native input layouts and default behavior remain
  compatible.

### Real-time design

- ScannedNetworkOsc allocates three double arrays at construction, uses a fixed
  1 kHz physical scheduler with conservative Verlet substeps, and scans the
  displacement array independently with cubic interpolation. Energy is
  evaluated on physical updates, not redundantly at audio rate.
- VectorPhaseOsc sanitizes one to eight breakpoint pairs every sample and
  combines oversampling with a maximum-segment-frequency Nyquist fade.
- SpectralFrameOsc has two construction-selected paths. Audio-rate spectral
  controls use the exact per-sample evaluator. Scalar/control-rate controls
  cook frame weights and increments once per block, ramp weights, and run
  recursive complex oscillators; inactive phase indices advance analytically
  without per-sample trigonometry.
- RippleFormantOsc integrates each chirp's instantaneous frequency and stops
  negligible ripples early.
- ShiftLogicOsc uses seeded xorshift decisions, exact integer register/DAC
  operations, and efficient BLAMP triangles.
- QuadratureFeedbackOsc uses filtered 1x/2x/4x/8x RK4 integration and rescales
  state only beyond radius four.
- SyncModeOsc processes sync before a simultaneous local wrap, applies BLEP/
  BLAMP waveform correction, and uses a fixed eight-slot causal correction
  queue for arbitrary phase jumps.
- TableMorph3D and TableMorph2D validate exact mono layouts and share spatial
  interpolation and mip selection.
- No new calculation callback allocates memory, performs file access, resizes
  state, or logs.

SpectralFrameOsc allocates eight double arrays, approximately
`64 * partials` bytes per instance. ScannedNetworkOsc allocates approximately
`24 * nodes` bytes per instance. Prepared Buffer memory is shared and is not
charged per UGen.

### Corrections found during runtime work

- Replaced an invalid Array `lace` call in SpectralFrameBuffer with
  transpose-and-flatten interleaving, producing the documented
  `[frame][partial][ratio, amplitude]` layout.
- A zero-multiplier RecordBuf test produced NaNs for scalar-folded constant
  channels in the test graph. The test now compares two deterministic
  ShiftLogicOsc instances under a nonzero affine transform, which checks all
  five post-UGen `mul`/`add` paths without introducing scalar channels.
- The first supernova binary was accidentally built against a newer
  SuperCollider interface (plugin API 5) and was correctly rejected by the
  installed 3.14.1 server (API 3). It was rebuilt and installed against the
  complete 3.14.1 SDK, after which the full additional-family supernova smoke
  test passed.
- The first benchmark exposed three unnecessary hot paths. Moving scanned
  energy to the physical cadence reduced the 512-node case by roughly 6x;
  replacing the additive triangle in ShiftLogicOsc with the shared BLAMP
  triangle reduced it by roughly 6x; block cooking and inactive-phase
  advancement made SpectralFrameOsc usable at hundreds of partials.
- One interrupted benchmark received a transient invalid
  `server.actualSampleRate` value from server status. Batch scaling now uses
  the explicitly requested rate, and the complete benchmark was rerun.

### v0.2 verification

- Strict-warning scsynth and API-compatible supernova builds pass.
- Expanded pure C++ reference tests pass for networks, phase segments,
  spectral frames, ripple integration, register/DAC behavior, Hopf
  integration, synchronization helpers, shared trilinear reading, and the 8x
  decimator.
- AddressSanitizer and UndefinedBehaviorSanitizer reference tests pass.
- Language construction, array rejection, all audio-rate-only `.kr`
  rejections, and installed class-name collision checks pass.
- The 30-channel installed scsynth smoke test covers all ten public oscillator
  additions, both SpectralFrameOsc execution paths, prepared buffers, output
  order, quadrature signs, ripple reconstruction, and five-channel affine
  `mul`/`add`.
- The installed supernova smoke test runs the entire additional family and
  produced finite non-silent output (`peak = 0.0141` in the recorded mix).
- The randomized safety test includes extreme values, negative and infinite
  frequency, rapid triggers, invalid metadata, and prepared buffers freed
  under active Synths. The server remained alive and synchronized.
- The 40-channel NRT matrix passed at 44.1, 48, 88.2, 96, and 192 kHz with
  block sizes 1, 16, 64, and 256. Quadrature sign error was exactly zero and
  ripple mix reconstruction error remained below `9.56e-8`.
- All 41 parenthesized help examples compile. A forced SCDoc re-index then
  parsed and rendered the 20 class pages and project guide without a warning
  or error.

### Final installed-state verification

The final optimized source state was rebuilt, installed into the SuperCollider
user extension directory, and tested there on 2026-07-30. The relevant commands
were:

```text
cmake --build build --parallel
cmake --build build-supernova --parallel
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
cmake --install build --prefix <SuperCollider user Extensions>
cmake --install build-supernova --prefix <SuperCollider user Extensions>
sclang tests/language/compile_check.scd
sclang tests/language/collision_check.scd
sclang tests/language/compile_test_scripts.scd
sclang tests/language/compile_help_examples.scd
sclang tests/language/additional_live_smoke.scd
sclang tests/language/additional_supernova_smoke.scd
sclang tests/language/randomized_safety.scd
sclang tests/nrt/render_metrics.scd
sclang tests/language/verify_scdoc.scd
```

Both strict release targets and the sanitizer reference target passed. The
installed scsynth smoke produced finite non-silent data on all 30 channels,
including both SpectralFrameOsc calculation paths. The installed supernova
smoke produced a finite peak of `0.0133`. Its startup printed pre-existing
warnings for unrelated installed VSTPlugin binaries and stale third-party
SynthDefs; NovelOscUGens loaded and completed normally.

The final NRT frequency estimates remained between `997.042` and `997.136` Hz,
the TZSplit reconstruction error remained `5.9604644775391e-08`, quadrature
sign error remained exactly zero, and ripple reconstruction error remained
below `9.56e-8`. `SCDoc.indexAllDocuments(true)` and explicit rendering of
every project page completed with `NOVEL_OSC_SCDOC_OK` and no SCDoc warning or
error.

### v0.2 real-time benchmark

Method matches the v0.1 benchmark: release scsynth, block size 64, local audio
driver block 512, requested rates 48/96 kHz, warmed batches, internal inaudible
bus output, and a no-synth baseline. The last column below is incremental
average CPU divided by instance count. These are comparative values for this
machine and run, not portable percentages.

#### 48 kHz

Baseline average CPU: `0.021`.

| Case | Instances | Average | Peak | Incremental average / instance |
|---|---:|---:|---:|---:|
| ScannedNetwork 32 nodes | 32 | 9.151 | 11.101 | 0.2853 |
| ScannedNetwork 128 nodes | 16 | 6.361 | 7.118 | 0.3963 |
| ScannedNetwork 512 nodes | 4 | 3.376 | 4.041 | 0.8386 |
| VectorPhase 1x | 64 | 15.592 | 17.370 | 0.2433 |
| VectorPhase 4x | 24 | 15.219 | 15.625 | 0.6333 |
| SpectralFrame 16 active / 512 stored | 32 | 35.146 | 36.782 | 1.0977 |
| SpectralFrame 128 active / 512 stored | 8 | 17.654 | 21.374 | 2.2041 |
| SpectralFrame 512 active / 512 stored | 2 | 11.919 | 18.089 | 5.9490 |
| RippleFormant 1x | 48 | 20.854 | 21.617 | 0.4340 |
| RippleFormant 4x | 16 | 27.246 | 29.039 | 1.7015 |
| ShiftLogic | 48 | 14.001 | 17.182 | 0.2912 |
| QuadratureFeedback 1x | 48 | 27.042 | 33.232 | 0.5629 |
| QuadratureFeedback 4x | 16 | 34.961 | 43.620 | 2.1837 |
| QuadratureFeedback 8x | 8 | 34.291 | 37.646 | 4.2837 |
| SyncMode 1x | 64 | 11.540 | 14.198 | 0.1800 |
| SyncMode 4x | 24 | 9.105 | 13.572 | 0.3785 |
| TableMorph3D cubic, 5 mips | 32 | 57.031 | 80.297 | 1.7815 |
| RatioFamily 6 voices | 16 | 3.551 | 4.310 | 0.2206 |
| RatioFamily 24 voices | 4 | 3.441 | 3.915 | 0.8550 |
| PitchRegister 3 voices | 12 | 25.171 | 27.018 | 2.0958 |
| PitchRegister 12 voices | 3 | 25.415 | 27.117 | 8.4645 |

#### 96 kHz

Baseline average CPU: `0.038`.

| Case | Instances | Average | Peak | Incremental average / instance |
|---|---:|---:|---:|---:|
| ScannedNetwork 32 nodes | 16 | 9.004 | 12.276 | 0.5604 |
| ScannedNetwork 128 nodes | 8 | 5.414 | 7.799 | 0.6721 |
| ScannedNetwork 512 nodes | 2 | 2.680 | 5.311 | 1.3209 |
| VectorPhase 1x | 32 | 16.062 | 19.404 | 0.5007 |
| VectorPhase 4x | 12 | 15.436 | 19.405 | 1.2832 |
| SpectralFrame 16 active / 512 stored | 16 | 35.403 | 37.056 | 2.2103 |
| SpectralFrame 128 active / 512 stored | 4 | 17.851 | 22.551 | 4.4532 |
| SpectralFrame 512 active / 512 stored | 1 | 12.385 | 15.649 | 12.3469 |
| RippleFormant 1x | 24 | 20.766 | 24.531 | 0.8637 |
| RippleFormant 4x | 8 | 27.433 | 36.171 | 3.4243 |
| ShiftLogic | 24 | 15.631 | 18.391 | 0.6497 |
| QuadratureFeedback 1x | 24 | 27.969 | 31.170 | 1.1638 |
| QuadratureFeedback 4x | 8 | 36.503 | 42.533 | 4.5581 |
| QuadratureFeedback 8x | 4 | 35.586 | 40.680 | 8.8870 |
| SyncMode 1x | 32 | 12.024 | 15.784 | 0.3746 |
| SyncMode 4x | 12 | 9.523 | 12.482 | 0.7904 |
| TableMorph3D cubic, 5 mips | 16 | 57.757 | 68.013 | 3.6074 |
| RatioFamily 6 voices | 8 | 3.533 | 5.740 | 0.4369 |
| RatioFamily 24 voices | 2 | 3.870 | 5.938 | 1.9159 |
| PitchRegister 3 voices | 6 | 25.353 | 28.231 | 4.2192 |
| PitchRegister 12 voices | 1 | 17.027 | 20.876 | 16.9893 |

The expected trends are visible: cost roughly doubles with sample rate;
oversampled nonlinear paths scale with factor; language-side family/register
graphs scale nearly linearly with voice count; ScannedNetwork rises with node
count; and SpectralFrame rises with active partials while retaining a smaller
block-cooking cost for stored inactive identities. Cubic TableMorph3D is one
of the heavier fixed-size readers because two mip levels require sixteen cubic
table reads per sample.

## 2026-08-19 acceptance and hardening audit

The complete additional-oscillator specification was checked against the
existing v0.2 source, language classes, help, guide, tests, packaging, and CI
configuration. All eight requested native UGens, the two requested
language-side constructors, and their documented support API were already
present; this pass therefore concentrated on boundary behavior and regression
coverage instead of creating duplicate implementations.

Hardening changes made during the audit:

- initialization and rounded control integers are now range-checked before
  conversion, avoiding undefined behavior for huge finite float inputs;
- VectorPhaseOsc breakpoints are sorted together with their associated phase
  values before strict spacing is applied;
- SpectralFrameBuffer, TableMorph2D, and TableMorph3D language-side preparation
  now enforce the native layout limits and reject non-finite sample data;
- reference coverage now includes breakpoint/value pairing, conservative
  network energy, the strike kernel, every trilinear table corner, and extreme
  integer-control inputs.

The final source state passed strict scsynth and supernova builds, the expanded
C++ reference suite, an ASan/UBSan reference build, language construction and
validation, all additional-oscillator live and randomized safety tests, and a
supernova smoke test. The 20-case NRT matrix passed at 44.1, 48, 88.2, 96, and
192 kHz with block sizes 1, 16, 64, and 256. A forced SCDoc re-index rendered
all 20 class pages and the project guide with no SCDoc warning or error.

Both server plug-ins and the updated documentation were installed to the local
SuperCollider user extension directory for immediate use. Hosted GitHub CI and
release publication were deliberately not run as part of this local audit.
