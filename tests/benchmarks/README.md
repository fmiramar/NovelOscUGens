# NovelOscUGens benchmarks

`realtime_cpu.scd` benchmarks installed release builds with the real-time
`scsynth` server at requested rates of 48 and 96 kHz. It measures a no-synth
baseline and then samples server average and peak CPU for batches of identical
instances.

`additional_realtime_cpu.scd` uses the same method for the v0.2 families. It
compares network node counts, spectral partial counts, 1x/4x/8x oversampling,
3D wavetable traversal, and small/large language-side RatioFamilyOsc and
PitchRegisterOsc graphs.

Run after building and installing the release plugin:

```sh
sclang tests/benchmarks/realtime_cpu.scd
sclang tests/benchmarks/additional_realtime_cpu.scd
```

The last column is `(case average CPU - baseline) / instances`. It is useful
for comparing cases from the same run; it is not a portable percentage of a
particular core. Audio hardware, server block scheduling, thermal state,
background processes, and unrelated installed plugins can all change the
numbers. The tiny signals are written to an internal audio bus so the complete
graphs remain active without audible output.

Cases expose:

- oscillator-family cost;
- 1x, 2x, and 4x oversampling for `TZSplitOsc`;
- 1x and 4x oversampling for `SyncRingOsc`;
- static versus audio-rate `PMNetworkOsc` routing;
- 8 versus 24 `ScaleSpreadOsc` voices;
- small versus large `SpectralBasisOsc` tables under refresh;
- 64 partials by one voice versus 256 partials by five voices.

`SpectralBasisOsc` and the optimized scalar/control-rate path of
`PartialClusterOsc` allocate variable-size private real-time storage at UGen
construction. Their allocations exclude the fixed UGen state and Synth graph.
`SpectralBasisOsc` uses approximately:

```text
(8 * mipLevels + 32) * tableSize bytes
```

At sizes 128 and 2048 this is about 8 KiB and 160 KiB respectively for the
current mip-count rule. `PartialClusterOsc` uses approximately:

```text
8 * maxPartials + 48 * voices * maxPartials bytes
```

The 64×1 and 256×5 benchmark cases therefore allocate about 3.5 KiB and
62 KiB per instance. Other classes use fixed UGen state or refer to shared
server Buffers; a Buffer's storage is not charged once per instance.

For the v0.2 cases, `ScannedNetworkOsc` owns about `24 * nodes` bytes and
`SpectralFrameOsc` owns about `64 * partials` bytes per instance. The
SpectralFrame benchmark deliberately keeps all shaping controls at scalar or
control rate, so it measures the block-cooked recursive path. Supplying any
audio-rate shaping control selects the exact per-sample path instead; the live
smoke test exercises both paths.

Measured local results and machine-independent interpretation are recorded in
`docs/implementation-log.md`.
