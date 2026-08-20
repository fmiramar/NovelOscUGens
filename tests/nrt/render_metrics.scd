(
var sampleRates = [44100, 48000, 88200, 96000, 192000];
var blockSizes = [1, 16, 64, 256];
var channelCount = 40;
var duration = 0.08;
var definition;

definition = SynthDef(\novelOscNRTMetrics, {
    var signals = Array.new;

    signals = signals ++ HarmonicStrideOsc.ar(
        997, decay: 0, maxPartials: 1
    );
    signals = signals ++ TZSplitOsc.ar(
        383, linearFM: SinOsc.ar(7, 0, 500).neg,
        phaseMod: SinOsc.ar(11, 0, 0.7),
        symmetry: 0.37, drive: 2.5,
        oversample: 4
    );
    signals = signals.add(SpectralBasisOsc.ar(
        421, basis: 3, center: 0.36,
        width: 0.19, tilt: -0.25,
        skew: 0.2, tableSize: 128
    ));
    signals = signals ++ PartialClusterOsc.ar(
        73, partials: 48, spacing: 1.07,
        warp: 0.08, tilt: 1.2,
        combPeriod: 5, combDepth: 0.3,
        voices: 3, detune: 0.5,
        phaseSpread: 0.4, maxPartials: 64
    );
    signals = signals ++ SyncRingOsc.ar(
        307, 2.5, 3.25, 0.7, 0.4,
        carrierWave: 2, modWave1: 1,
        modWave2: 3, oversample: 4
    );
    signals = signals ++ PMNetworkOsc.ar(
        191, [1, 1.5, 2],
        [
            0, 0.5, 0,
            0, 0, 0.35,
            0.2, 0, 0
        ],
        [
            0, 8, 0,
            0, 0, 5,
            3, 0, 0
        ],
        [1, 0.7, 0.5], [0, 2, 1], 4
    );
    signals = signals ++ ClusterPMOsc.ar(
        127, [1, 1.5, 2, 3],
        [1, 1.01, 2, 3],
        [1, 0.3, 0.15, 0.08],
        [
            0, 0.4, 0, 0,
            0, 0, 0.3, 0,
            0.2, 0, 0, 0,
            0, 0.1, 0, 0
        ],
        [1, 0.6, 0.4, 0.25], 4
    );
    signals = signals ++ ScannedNetworkOsc.ar(
        173, rate: 0.2, stiffness: 0.35,
        strike: Impulse.ar(211), strikeAmp: 0.1,
        nodes: 32
    );
    signals = signals.add(VectorPhaseOsc.ar(
        281, [0.18, 0.63], [0.4, 0.82],
        oversample: 4
    ));
    signals = signals ++ RippleFormantOsc.ar(
        107, 1100, 2700, 0.009, 0.004,
        0.7, -0.5, oversample: 4
    );
    signals = signals ++ ShiftLogicOsc.ar(
        149, 197, 0.6, -0.4,
        change: 0.75, seed: 1729
    );
    signals = signals ++ QuadratureFeedbackOsc.ar(
        233, SinOsc.ar(19, 0, 0.03),
        [0.25, -0.15, 0.1, -0.05],
        inject: 0.3, stability: 1.4,
        drive: 2.2, oversample: 8
    );
    signals = signals.add(SyncModeOsc.ar(
        263, Impulse.ar(317), 3,
        resetPhase: 0.2, softness: 0.002,
        shape: 2, oversample: 4
    ));
    signals = signals ++ RatioFamilyOsc.ar(
        157, [1, 1.5, 2],
        spread: 0.8, skew: 0.34, curve: 0.3
    );
    Out.ar(0, signals)
});

Routine({
    var summaries = List.new;

    try {
        sampleRates.do { |sampleRate|
            blockSizes.do { |blockSize|
                var outputPath = Platform.defaultTempDir +/+
                    "novel-osc-nrt-%-%.wav".format(
                        sampleRate, blockSize
                    );
                var options = ServerOptions.new;
                var score;
                var completed = FlowVar.new;
                var soundFile;
                var data;
                var frames;
                var channelData;
                var peaks;
                var crossings = List.new;
                var frequency;
                var reconstructionError;
                var spectralMean;
                var quadratureError;
                var rippleError;

                options.numOutputBusChannels = channelCount;
                options.numInputBusChannels = 0;
                options.numAudioBusChannels = 256;
                options.numWireBufs = 2048;
                options.memSize = 131072;
                options.blockSize = blockSize;

                score = Score([
                    [0.0, [\d_recv, definition.asBytes]],
                    [0.0, [
                        \s_new, \novelOscNRTMetrics,
                        1000, 0, 0
                    ]],
                    [duration, [\n_free, 1000]]
                ]);
                "NRT_RENDER_START % Hz block %"
                .format(sampleRate, blockSize).postln;
                score.recordNRT(
                    outputFilePath: outputPath,
                    sampleRate: sampleRate,
                    headerFormat: "wav",
                    sampleFormat: "float",
                    options: options,
                    duration: duration,
                    action: {
                        "NRT_RENDER_DONE % Hz block %"
                        .format(sampleRate, blockSize)
                        .postln;
                        completed.value = true
                    }
                );
                completed.value;

                soundFile = SoundFile.openRead(outputPath);
                if(soundFile.isOpen.not) {
                    Error(
                        "Could not open NRT render %."
                        .format(outputPath)
                    ).throw
                };
                frames = soundFile.numFrames;
                if(soundFile.numChannels != channelCount) {
                    Error("NRT channel count mismatch.").throw
                };
                data = FloatArray.newClear(
                    frames * channelCount
                );
                soundFile.readData(data);
                soundFile.close;
                File.delete(outputPath);

                if(data.any { |value|
                    value.isNaN or: { value.abs >= inf }
                }) {
                    Error(
                        "NRT render emitted NaN or infinity."
                    ).throw
                };
                channelData = { |channel|
                    FloatArray.fill(frames, { |frame|
                        data[
                            (frame * channelCount) + channel
                        ]
                    })
                };
                peaks = Array.fill(channelCount, { |channel|
                    channelData.(channel).abs.maxItem
                });
                [
                    [0, 1], [2, 3, 4, 5], [6],
                    [7, 8], [9, 10, 11, 12],
                    [13, 14, 15], [16, 17, 18, 19],
                    [20, 21], [22], [23, 24, 25, 26],
                    [27, 28, 29, 30, 31],
                    [32, 33, 34, 35], [36], [37, 38, 39]
                ].do { |group|
                    if(group.collect { |channel|
                        peaks[channel]
                    }.maxItem < 1e-5) {
                        Error(
                            "An NRT oscillator group stayed silent."
                        ).throw
                    }
                };

                (1..(frames - 1)).do { |frame|
                    var previous = data[
                        ((frame - 1) * channelCount)
                    ];
                    var current = data[
                        frame * channelCount
                    ];
                    if(previous <= 0 and: { current > 0 }) {
                        crossings.add(frame)
                    }
                };
                if(crossings.size < 3) {
                    Error(
                        "Too few HarmonicStrideOsc crossings."
                    ).throw
                };
                frequency = (
                    (crossings.size - 1) * sampleRate
                    / (crossings.last - crossings.first)
                );
                if((frequency - 997).abs > 1.5) {
                    Error(
                        "NRT frequency mismatch: % Hz."
                        .format(frequency)
                    ).throw
                };

                reconstructionError = Array.fill(
                    frames, { |frame|
                        (
                            data[
                                (frame * channelCount) + 3
                            ]
                            + data[
                                (frame * channelCount) + 4
                            ]
                            - data[
                                (frame * channelCount) + 5
                            ]
                        ).abs
                    }
                ).maxItem;
                if(reconstructionError > 3e-5) {
                    Error(
                        "NRT TZ reconstruction error %."
                        .format(reconstructionError)
                    ).throw
                };

                spectralMean = channelData.(6).mean.abs;
                if(spectralMean > 0.15) {
                    Error(
                        "SpectralBasisOsc NRT mean is too large: %."
                        .format(spectralMean)
                    ).throw
                };

                quadratureError = Array.fill(
                    frames, { |frame|
                        (
                            data[(frame * channelCount) + 32]
                            + data[(frame * channelCount) + 34]
                        ).abs.max(
                            (
                                data[(frame * channelCount) + 33]
                                + data[(frame * channelCount) + 35]
                            ).abs
                        )
                    }
                ).maxItem;
                if(quadratureError > 3e-6) {
                    Error(
                        "Quadrature NRT sign error %."
                        .format(quadratureError)
                    ).throw
                };

                rippleError = Array.fill(
                    frames, { |frame|
                        var expected = (
                            data[(frame * channelCount) + 23] * 0.2
                        ) + (
                            (
                                data[(frame * channelCount) + 24]
                                + data[(frame * channelCount) + 25]
                            ) * 2.sqrt.reciprocal
                        );
                        (
                            data[(frame * channelCount) + 26]
                            - expected
                        ).abs
                    }
                ).maxItem;
                if(rippleError > 3e-5) {
                    Error(
                        "Ripple NRT reconstruction error %."
                        .format(rippleError)
                    ).throw
                };

                summaries.add([
                    sampleRate, blockSize,
                    frequency.round(0.001),
                    reconstructionError,
                    peaks.maxItem.round(0.0001),
                    spectralMean.round(0.000001),
                    quadratureError, rippleError
                ]);
            }
        };

        "NOVEL_OSC_NRT_METRICS_OK".postln;
        "sampleRate blockSize measuredHz tzError peak spectralAbsMean quadratureError rippleError"
        .postln;
        summaries.do(_.postln);
        0.exit;
    } { |error|
        error.reportError;
        1.exit;
    }
}).play(SystemClock);
)
