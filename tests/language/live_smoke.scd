(
var server = Server(
    \novelOscLiveSmoke,
    NetAddr("127.0.0.1", 57129)
);

server.options.numAudioBusChannels = 256;
server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 65536;

server.waitForBoot({
    Routine({
        var tableSize = 64;
        var tables;
        var tableBuffer;
        var scaleBuffer;
        var recordBuffer;
        var data;
        var condition;
        var channels = 25;
        var frames = 4096;
        var peaks;
        var channelData;
        var maximumDifference;

        try {
            tables = [
                Signal.sineFill(tableSize, [1]),
                Signal.sineFill(tableSize, [1, 0.25]),
                Signal.sineFill(tableSize, [0, 1]),
                Signal.sineFill(tableSize, [1, 0, 0.2])
            ];
            tableBuffer = TableMorph2D.prepareBuffer(
                server, tables, 2, 2, 3
            );
            scaleBuffer = ScaleSpreadOsc.prepareScale(
                server, [1, 9/8, 5/4, 4/3, 3/2, 5/3, 15/8]
            );
            recordBuffer = Buffer.alloc(
                server, frames, channels
            );
            server.sync;

            if(tableBuffer.numFrames !=
                (tableSize * 2 * 2 * 3)) {
                Error(
                    "TableMorph2D.prepareBuffer produced the wrong layout."
                ).throw
            };
            if(scaleBuffer.numFrames != 7) {
                Error(
                    "ScaleSpreadOsc.prepareScale produced the wrong layout."
                ).throw
            };

            SynthDef(\novelOscLiveSmoke, {
                |recordBuf, tableBuf, scaleBuf|
                var signals = Array.new;

                signals = signals ++ HarmonicStrideOsc.ar(
                    110, 1.25, 0.65, maxPartials: 64
                );
                signals = signals.add(TableMorph2D.ar(
                    tableBuf, 165, 0.35, 0.6,
                    tablesX: 2, tablesY: 2,
                    tableSize: 64, mipLevels: 3
                ));
                signals = signals ++ TZSplitOsc.ar(
                    97, symmetry: 0.35, drive: 1.5
                );
                signals = signals ++ ScaleSpreadOsc.ar(
                    scaleBuf, 73, center: 2.2,
                    spread: 0.75, voices: 5,
                    modulation: 0.1, stereo: 0
                );
                signals = signals.add(SpectralBasisOsc.ar(
                    123, basis: 3, center: 0.37,
                    width: 0.21, tableSize: 128
                ));
                signals = signals ++ PartialClusterOsc.ar(
                    61, partials: 24, spacing: 1.1,
                    warp: 0.15, tilt: 1.1,
                    combPeriod: 3, combDepth: 0.35,
                    voices: 3, detune: 0.8,
                    phaseSpread: 0.2, maxPartials: 64
                );
                signals = signals ++ SyncRingOsc.ar(
                    89, 2.5, 3.25, 0.6, 0.35,
                    carrierWave: 2, modWave1: 1,
                    modWave2: 0
                );
                signals = signals ++ PMNetworkOsc.ar(
                    71,
                    pm: [
                        0, 0.2, 0,
                        0, 0, 0.15,
                        0.1, 0, 0
                    ],
                    fm: [
                        0, 3, 0,
                        0, 0, 2,
                        1, 0, 0
                    ],
                    waveforms: [0, 2, 1]
                );
                signals = signals ++ ClusterPMOsc.ar(
                    53,
                    pm: [
                        0, 0.12, 0, 0,
                        0, 0, 0.08, 0,
                        0.05, 0, 0, 0,
                        0, 0, 0.04, 0
                    ]
                );
                signals = signals ++ PartialClusterOsc.ar(
                    67, partials: 24,
                    warp: SinOsc.ar(7, 0, 0.08),
                    voices: 2, maxPartials: 32
                );
                RecordBuf.ar(
                    signals, recordBuf,
                    loop: 0, doneAction: Done.freeSelf
                )
            }).add;
            server.sync;

            Synth(\novelOscLiveSmoke, [
                \recordBuf, recordBuffer,
                \tableBuf, tableBuffer,
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

            if(data.size != (frames * channels)) {
                Error("Recorded smoke data has the wrong size.").throw
            };
            if(data.any { |value|
                value.isNaN or: { value.abs >= inf }
            }) {
                Error("A NovelOscUGen emitted NaN or infinity.").throw
            };

            channelData = { |channel|
                FloatArray.fill(frames, { |frame|
                    data[(frame * channels) + channel]
                })
            };
            peaks = Array.fill(channels, { |channel|
                channelData.(channel).abs.maxItem
            });

            [
                [0, 1], [2], [3, 4, 5, 6], [7, 8],
                [9], [10, 11], [12, 13, 14, 15],
                [16, 17, 18], [19, 20, 21, 22],
                [23, 24]
            ].do { |group, index|
                if(group.collect { |channel|
                    peaks[channel]
                }.maxItem < 1e-5) {
                    Error(
                        "Novel oscillator group % stayed silent."
                        .format(index)
                    ).throw
                }
            };

            maximumDifference = Array.fill(frames, { |frame|
                (
                    data[(frame * channels) + 4]
                    + data[(frame * channels) + 5]
                    - data[(frame * channels) + 6]
                ).abs
            }).maxItem;
            if(maximumDifference > 2e-5) {
                Error(
                    "TZSplitOsc even + odd != full (max error %)."
                    .format(maximumDifference)
                ).throw
            };

            maximumDifference = Array.fill(frames, { |frame|
                (
                    data[(frame * channels) + 7]
                    - data[(frame * channels) + 8]
                ).abs
            }).maxItem;
            if(maximumDifference > 2e-6) {
                Error(
                    "ScaleSpreadOsc stereo=0 did not produce matching channels."
                ).throw
            };

            "NOVEL_OSC_LIVE_SMOKE_OK peaks=%"
            .format(peaks.round(0.0001)).postln;

            [recordBuffer, tableBuffer, scaleBuffer].do(_.free);
            server.sync;
            server.quit;
            0.exit;
        } { |error|
            error.reportError;
            [recordBuffer, tableBuffer, scaleBuffer].do {
                |buffer|
                if(buffer.notNil) { buffer.free }
            };
            server.quit;
            1.exit;
        }
    }).play(SystemClock)
}, 15, {
    "NovelOscUGens live smoke server failed to boot.".error;
    1.exit;
});

server.boot;
)
