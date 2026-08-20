# NovelOscUGens

NovelOscUGens is a standalone SuperCollider server-plugin extension by
[fmiramar](https://github.com/fmiramar). It provides seventeen experimental
audio-rate server UGens and two language-side oscillator constructors for
phase shaping, wavetable morphing, additive spectra, feedback networks, and
nonlinear modulation.

This is an original clean-room collection, not a port of one named instrument
or plug-in. Its DSP combines published mathematics and established synthesis
techniques—geometric series, wavetable interpolation, BLEP/BLAMP correction,
orthogonal transforms, additive synthesis, signed phase accumulation, and
modulation matrices—into SuperCollider-native designs. No proprietary
firmware, extracted binary data, factory wavetable, preset, panel artwork, or
copied product documentation is included.

## UGens

- `HarmonicStrideOsc` creates a geometrically weighted harmonic or fractional-stride series and returns quadrature outputs. A stable finite complex sum replaces a bank of independently running oscillators.

- `TableMorph2D` scans a two-dimensional surface of user-supplied wavetables. It bilinearly interpolates four tables and crossfades prepared mip levels according to playback frequency.

- `TZSplitOsc` is a signed through-zero oscillator with sine, even, odd, and full outputs. It combines a BLAMP-corrected asymmetric triangle, oversampled nonlinear drive, and exact even/odd reconstruction.

- `ScaleSpreadOsc` distributes a fixed stereo oscillator bank over an arbitrary ratio scale. Each voice equal-power crossfades between adjacent scale degrees and may receive nearest-neighbor phase modulation.

- `SpectralBasisOsc` constructs an internal wavetable from Fourier, Walsh, Haar, or Daubechies-4 basis coefficients. It regenerates an inactive mipmapped bank at block boundaries and crossfades without allocating in the audio callback.

- `PartialClusterOsc` produces a large procedurally warped additive spectrum with spectral tilt, smooth combing, deterministic phase spread, and unison. Active components are smoothly culled at Nyquist and normalized from their actual amplitude sum.

- `SyncRingOsc` combines one carrier with two phase-synchronized ring modulators. Integer and fractional ratios share deterministic carrier-cycle timing, with bandlimited shapes and optional oversampling.

- `PMNetworkOsc` provides three operators connected by independent row-major PM and through-zero linear-FM matrices. Feedback reads the previous oversampled substep, defining cyclic networks without algebraic loops.

- `ClusterPMOsc` provides four PM operators whose local oscillators are normalized four-component additive clusters. The shared cluster spectrum and a row-major feedback matrix produce denser modulation than a sine-only network.

- `ScannedNetworkOsc` scans a slowly evolving one-dimensional mass-spring network as a waveform. A separately scheduled, substepped Verlet integration keeps physical timbre motion independent of signed audio-rate scanning.

- `VectorPhaseOsc` bends phase through as many as eight movable two-dimensional breakpoints. Strictly ordered piecewise segments, oversampling, and a segment-slope Nyquist fade keep the mapping robust under modulation.

- `SpectralFrameOsc` resynthesizes Buffer-stored ratio-and-amplitude frames with continuous interpolation. Independent signed partial phases, spectral shaping, activation fades, Nyquist culling, and energy normalization preserve continuity.

- `RippleFormantOsc` launches two decaying chirped ripples on every base cycle and exposes their components and mix. Ripple phase is integrated from instantaneous frequency inside an optionally oversampled path.

- `ShiftLogicOsc` couples two triangle oscillators through a seeded shift register and bipolar DAC. Register events, feedback frequency changes, and five outputs remain sample coherent.

- `QuadratureFeedbackOsc` returns four directions of a regulated nonlinear two-state orbit. Filtered RK4 oversampling and a safety radius limiter provide a bounded state-space feedback instrument.

- `SyncModeOsc` gives sine, triangle, saw, or pulse playback six explicit synchronization laws. BLEP/BLAMP shapes, causal event correction, and defined sync-before-wrap ordering reduce ambiguity and aliasing.

- `TableMorph3D` scans a prepared three-dimensional wavetable volume. It shares interpolation and mip selection with `TableMorph2D` and adds trilinear spatial blending.

- `RatioFamilyOsc` builds one phase-shaped oscillator per literal ratio. Logarithmic spread moves continuously through unison, supplied, expanded, and reciprocal relationships without a dedicated server primitive.

- `PitchRegisterOsc` shifts triggered scale degrees through a fixed register and sonifies every position. A minimal native register preserves sample timing while ordinary language-side UGens perform scale conversion, glide, and oscillator construction.

`SpectralFrameBuffer` validates and loads deterministic spectral-frame data and
keeps frame and partial metadata in a language object.

All oscillator classes are audio-rate only. Calling `.kr` throws deliberately.

## Requirements

- SuperCollider 3.14 or a compatible current server-plugin API
- CMake 3.12 or newer
- A C++17 compiler
- A matching SuperCollider source checkout supplied as `SC_PATH`

No Quark or other third-party runtime dependency is required.

## Install a release

Download the archive for your operating system and CPU from the
[project releases](https://github.com/fmiramar/NovelOscUGens/releases), then
copy the contained `NovelOscUGens` folder to your SuperCollider user Extensions
directory. Restart SuperCollider or recompile the class library afterwards.
The extension folder contains the plug-in binary, classes, help, license, and
project documentation; keep that folder intact.

Release archives use the form
`fmiramar-NovelOscUGens-<version>-<platform>-<architecture>.zip`. For example,
an Intel Mac download is named
`fmiramar-NovelOscUGens-0.2.0-macos-x86_64.zip`.

## Build and install

Configure out of tree. `SC_PATH` must refer to source matching the installed
server closely enough to provide compatible plugin headers.

```sh
cmake -S . -B build \
  -DSC_PATH=/path/to/supercollider \
  -DCMAKE_BUILD_TYPE=Release \
  -DSCSYNTH=ON \
  -DSUPERNOVA=OFF \
  -DSTRICT=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix "/path/to/SuperCollider/Extensions"
```

Set `SUPERNOVA=ON` when building against a source tree that includes a
compatible Boost lockfree setup. Recompile the SuperCollider class library
after installing. The included `scripts/build-macos.sh`, `build-linux.sh`, and
`build-windows.ps1` scripts run the corresponding configure, build, test, and
stage sequence.

The scripts accept the SuperCollider source path and optional build and install
locations. The automated build matrix is in `.github/workflows/build.yml`.

## Buffer preparation

`TableMorph2D.prepareBuffer` accepts a row-major collection of equal-size
tables, removes DC, optionally normalizes them, constructs low-pass mip levels,
and loads the exact `[mip][y][x][sample]` layout expected by the server UGen.

`ScaleSpreadOsc.prepareScale` loads a mono list of positive, strictly ascending
ratios beginning with `1`.

`TableMorph3D.prepareBuffer` prepares `[mip][z][y][x][sample]` data through the
same mip builder as TableMorph2D. `SpectralFrameBuffer.prepare` validates and
optionally energy-normalizes `[frame][partial][ratio, amplitude]` data.
Preparation rejects nonfinite table samples and layouts that exceed the native
dimension or server Buffer sample limits before sending allocation commands.

These operations send asynchronous buffer commands. Use a `Routine`, call
`s.sync`, and only then start a Synth. The installed help files contain complete
runnable examples.

## Verification

The repository contains:

- strict C++ reference tests for phase, interpolation, transforms, bandlimiting, dynamic networks, phase segments, spectral frames, ripple integration, register logic, Hopf integration, synchronization helpers, trilinear interpolation, and finite-output guards;
- language tests for class compilation, `.ar`, `.kr`, fixed arrays, and buffer preparation;
- installed-plugin live smoke tests that record 25 v0.1 outputs and 30 v0.2 outputs, cover the exact and optimized spectral-bank paths, and check finite data, non-silence, reconstruction identities, output ordering, and affine `mul`/`add`;
- NRT spectral/continuity tests and randomized server-safety tests;
- release server benchmarks at 48 and 96 kHz.

See `docs/implementation-log.md` for commands, measured results, and known
coverage limits.

## Status and limitations

Version `0.2.0` is experimental. The APIs and sound still need revision cycles
in real compositions before a `1.0` claim.

- High partial counts, high voice counts, 4x oversampling, and large spectral tables can be expensive. `PartialClusterOsc` uses a faster recursive bank for scalar/control-rate shaping and an exact but much costlier per-sample path when any shaping input is audio rate; benchmark representative patches on the target machine.
- `SpectralBasisOsc` performs bounded table regeneration in an audio calculation block. The path allocates no memory, but very large tables at high refresh rates may cause a CPU spike.
- `TableMorph2D` and `ScaleSpreadOsc` make an active instance permanently silent if its prepared buffer is freed or reallocated.
- `TableMorph3D` and `SpectralFrameOsc` follow the same permanent-silence policy for missing or reallocated prepared buffers.
- `ScannedNetworkOsc` and `SpectralFrameOsc` are intentionally O(N) in node or active-partial count. The former allocates three double arrays (about `24 * nodes` bytes) per instance; the latter allocates eight double arrays (about `64 * partials` bytes) per instance. Spectral controls that are all scalar/control rate select a block-cooked recursive bank, while any audio-rate spectral control selects the exact per-sample path.
- Causal sync correction and oversampling reduce discontinuity and nonlinear aliases but do not make extreme hard-sync or phase-shaping settings mathematically alias free.
- Builds target Linux, macOS, and Windows, but each release should still be
  exercised on the exact SuperCollider versions shipped with its artifacts.

## Source and attribution

`ORIGINS.md` classifies each clean-room design. The public SuperCollider plugin
API and repository conventions are the only external build-time source
relationship; `THIRD_PARTY_NOTICES.md` records that relationship. No upstream
DSP source has been incorporated.

## License

NovelOscUGens is licensed under the GNU General Public License, version 3 or
any later version. See `LICENSE`.
