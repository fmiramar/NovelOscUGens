# Changelog

## 0.2.0 - 2026-08-20

- Added eight public server UGens: ScannedNetworkOsc, VectorPhaseOsc,
  SpectralFrameOsc, RippleFormantOsc, ShiftLogicOsc,
  QuadratureFeedbackOsc, SyncModeOsc, and TableMorph3D.
- Added the RatioFamilyOsc and PitchRegisterOsc language-side graph
  constructors plus SpectralFrameBuffer preparation metadata.
- Added a minimal internal audio-rate pitch register after block-level
  LocalBuf/Demand ordering analysis showed that it could not guarantee the
  documented multi-trigger timing.
- Added shared dynamic-network, phase-segment, spectral-frame, ripple,
  shift-register, Hopf, sync-event, and 2D/3D wavetable infrastructure.
- Added final `mul` and `add` arguments to the original nine language wrappers
  without changing their native plugin inputs.
- Extended tests, help, benchmarks, origins, and CI metadata for v0.2.0.
- Hardened initialization-integer conversion against huge finite values,
  sorted VectorPhaseOsc breakpoint pairs without detaching their y values,
  and added language-side frame/table bounds plus finite-sample validation.

## 0.1.0 - unreleased

- Added all nine clean-room audio-rate UGen implementations and language
  classes.
- Added shared signed-phase, BLEP/BLAMP, interpolation, transform, buffer
  validation, oscillator-shape, and oversampling DSP helpers.
- Added prepared 2D wavetable and scale-buffer language APIs.
- Added strict reference, language, live-server, NRT, randomized-safety, and
  performance test harnesses.
- Added complete class help, the Novel Oscillators guide, origins and
  third-party notices, cross-platform build scripts, and CI.
