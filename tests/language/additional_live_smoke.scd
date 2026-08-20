(
var server = Server(
    \novelOscAdditionalSmoke,
    NetAddr("127.0.0.1", 57139)
);

server.options.numAudioBusChannels = 256;
server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 131072;

server.waitForBoot({
    Routine({
        var size = 64;
        var tableBuffer;
        var spectralBank;
        var scaleBuffer;
        var recordBuffer;
        var frames = 4096;
        var channelCount = 30;
        var data;
        var condition;
        var peaks;
        var channelData;
        var maximumError;
        var badIndex;

        try {
            tableBuffer = TableMorph3D.prepareBuffer(
                server,
                8.collect { |index|
                    Signal.sineFill(
                        size, [1, index / 16, index / 40]
                    )
                },
                2, 2, 2, 3
            );
            SpectralFrameBuffer.prepare(
                server,
                [
                    [[1, 2, 3, 4], [1, 0.5, 0.25, 0.1]],
                    [[1, 1.5, 2.7, 4.2], [1, 0.7, 0.2, 0.08]]
                ],
                action: { |object| spectralBank = object }
            );
            scaleBuffer = Buffer.loadCollection(
                server, [0, 2, 3, 5, 7, 8, 10]
            );
            recordBuffer = Buffer.alloc(
                server, frames, channelCount
            );
            server.sync;

            if(tableBuffer.numFrames != (size * 8 * 3)) {
                Error("TableMorph3D prepared layout mismatch.").throw
            };
            if(spectralBank.isNil
                or: { spectralBank.frames != 2 }
                or: { spectralBank.partials != 4 }
                or: { spectralBank.buffer.numFrames != 16 }) {
                Error("SpectralFrameBuffer metadata mismatch.").throw
            };

            SynthDef(\novelOscAdditionalSmoke, {
                |recordBuf, tableBuf, spectralBuf, scaleBuf|
                var signals = Array.new;
                var pitchTrigger = Impulse.ar(11);
                var pitchDegree = Demand.ar(
                    pitchTrigger, 0,
                    Dseq([0, 2, 4, 7, 5, 3], inf)
                );

                signals = signals ++ ScannedNetworkOsc.ar(
                    73, rate: 0.3, stiffness: 0.4,
                    strike: Impulse.ar(13), strikeAmp: 0.2,
                    nodes: 48
                );
                signals = signals.add(VectorPhaseOsc.ar(
                    83, [0.2, 0.65], [0.35, 0.82],
                    oversample: 4
                ));
                signals = signals.add(
                    SpectralFrameOsc.ar(
                        spectralBuf, 67,
                        frame: LFTri.kr(0.4).range(0, 1),
                        frames: 2, partials: 4
                    )
                    + (
                        SpectralFrameOsc.ar(
                            spectralBuf, 71,
                            frame: SinOsc.ar(3).range(0, 1),
                            stretch: SinOsc.ar(2).range(0.8, 1.2),
                            frames: 2, partials: 4
                        ) * 0.1
                    )
                );
                signals = signals ++ RippleFormantOsc.ar(
                    61, 530, 1700, 0.02, 0.007,
                    0.5, -0.3, oversample: 4
                );
                signals = signals ++ ShiftLogicOsc.ar(
                    59, 79, 0.8, -0.5,
                    change: 0.7, seed: 1234
                );
                signals = signals ++ QuadratureFeedbackOsc.ar(
                    71, SinOsc.ar(31, 0, 0.05),
                    [0.3, -0.2, 0.1, -0.1],
                    inject: 0.4, stability: 1.5,
                    drive: 2, oversample: 8
                );
                signals = signals.add(SyncModeOsc.ar(
                    89, Impulse.ar(17), 2,
                    shape: 2, oversample: 4
                ));
                signals = signals.add(TableMorph3D.ar(
                    tableBuf, 97, 0.27, 0.53, 0.81,
                    tablesX: 2, tablesY: 2, tablesZ: 2,
                    tableSize: 64, mipLevels: 3
                ));
                signals = signals ++ RatioFamilyOsc.ar(
                    47, [1, 1.5, 2], spread: 0.8,
                    skew: 0.37, curve: 0.25
                );
                signals = signals ++ PitchRegisterOsc.ar(
                    pitchTrigger, pitchDegree, scaleBuf, 43,
                    initialDegrees: [0, -2, -4],
                    harmonics: 12
                );
                signals = signals ++ ShiftLogicOsc.ar(
                    59, 79, 0.8, -0.5,
                    change: 0.7, seed: 1234,
                    mul: 0.5, add: 0.125
                );
                RecordBuf.ar(
                    signals, recordBuf,
                    loop: 0, doneAction: Done.freeSelf
                )
            }).add;
            server.sync;

            Synth(\novelOscAdditionalSmoke, [
                \recordBuf, recordBuffer,
                \tableBuf, tableBuffer,
                \spectralBuf, spectralBank.buffer,
                \scaleBuf, scaleBuffer
            ], server);
            0.25.wait;
            server.sync;

            condition = Condition({ data.notNil });
            recordBuffer.loadToFloatArray(
                action: { |array|
                    data = array;
                    condition.signal
                }
            );
            condition.wait;
            if(data.size != (frames * channelCount)) {
                Error("Additional smoke data size mismatch.").throw
            };
            badIndex = data.detectIndex { |value|
                value.isNaN or: { value.abs >= inf }
            };
            if(badIndex.notNil) {
                Error(
                    "An additional oscillator emitted % at frame %, channel %."
                    .format(
                        data[badIndex],
                        badIndex.div(channelCount),
                        badIndex % channelCount
                    )
                ).throw
            };
            channelData = { |channel|
                FloatArray.fill(frames, { |frame|
                    data[(frame * channelCount) + channel]
                })
            };
            peaks = Array.fill(channelCount, { |channel|
                channelData.(channel).abs.maxItem
            });
            [
                [0, 1], [2], [3], [4, 5, 6, 7],
                [8, 9, 10, 11, 12], [13, 14, 15, 16],
                [17], [18], [19, 20, 21], [22, 23, 24]
            ].do { |group, index|
                if(group.collect { |channel|
                    peaks[channel]
                }.maxItem < 1e-5) {
                    Error(
                        "Additional oscillator group % stayed silent."
                        .format(index)
                    ).throw
                }
            };

            maximumError = Array.fill(frames, { |frame|
                (
                    data[(frame * channelCount) + 13]
                    + data[(frame * channelCount) + 15]
                ).abs.max(
                    (
                        data[(frame * channelCount) + 14]
                        + data[(frame * channelCount) + 16]
                    ).abs
                )
            }).maxItem;
            if(maximumError > 2e-6) {
                Error(
                    "Quadrature sign outputs differ by %."
                    .format(maximumError)
                ).throw
            };
            maximumError = Array.fill(frames, { |frame|
                var expected = (
                    data[(frame * channelCount) + 4] * 0.2
                ) + (
                    (
                        data[(frame * channelCount) + 5]
                        + data[(frame * channelCount) + 6]
                    ) * 2.sqrt.reciprocal
                );
                (
                    data[(frame * channelCount) + 7]
                    - expected
                ).abs
            }).maxItem;
            if(maximumError > 3e-5) {
                Error(
                    "RippleFormantOsc mix reconstruction differs by %."
                    .format(maximumError)
                ).throw
            };
            (25..29).do { |channel|
                maximumError = Array.fill(frames, { |frame|
                    (
                        data[(frame * channelCount) + channel]
                        - (
                            data[
                                (frame * channelCount)
                                + channel - 17
                            ] * 0.5
                        )
                        - 0.125
                    ).abs
                }).maxItem;
                if(maximumError > 1e-7) {
                    Error(
                        "Multi-output mul/add failed on channel %."
                        .format(channel)
                    ).throw
                }
            };

            "NOVEL_OSC_ADDITIONAL_LIVE_SMOKE_OK peaks=%"
            .format(peaks.round(0.0001)).postln;
            [
                recordBuffer, tableBuffer,
                spectralBank.buffer, scaleBuffer
            ].do(_.free);
            server.sync;
            server.quit;
            0.exit;
        } { |error|
            error.reportError;
            [
                recordBuffer, tableBuffer,
                if(spectralBank.notNil) {
                    spectralBank.buffer
                } { nil },
                scaleBuffer
            ].do { |buffer|
                if(buffer.notNil) { buffer.free }
            };
            server.quit;
            1.exit;
        }
    }).play(SystemClock)
}, 20, {
    "NovelOscUGens additional smoke server failed to boot.".error;
    1.exit;
});

server.boot;
)
