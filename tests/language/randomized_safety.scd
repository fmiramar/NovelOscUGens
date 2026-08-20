(
var server = Server(
    \novelOscRandomizedSafety,
    NetAddr("127.0.0.1", 57131)
);

server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 256;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 131072;

server.waitForBoot({
    Routine({
        var tableSize = 64;
        var tableBuffer;
        var table3DBuffer;
        var spectralBank;
        var scaleBuffer;
        var replacement;
        var synth;
        var invalidSynth;

        try {
            thisThread.randSeed = 16273;
            tableBuffer = TableMorph2D.prepareBuffer(
                server,
                [
                    Signal.sineFill(tableSize, [1]),
                    Signal.sineFill(tableSize, [1, 0.2]),
                    Signal.sineFill(tableSize, [0, 1]),
                    Signal.sineFill(tableSize, [1, 0, 0.2])
                ],
                2, 2, 3
            );
            table3DBuffer = TableMorph3D.prepareBuffer(
                server,
                8.collect { |index|
                    Signal.sineFill(
                        tableSize, [1, index / 20]
                    )
                },
                2, 2, 2, 3
            );
            SpectralFrameBuffer.prepare(
                server,
                [
                    [[1, 2, 3, 4], [1, 0.5, 0.2, 0.1]],
                    [[1, 1.7, 2.8, 4.5], [1, 0.3, 0.7, 0.08]]
                ],
                action: { |object| spectralBank = object }
            );
            scaleBuffer = ScaleSpreadOsc.prepareScale(
                server, [1, 9/8, 5/4, 4/3, 3/2, 5/3]
            );
            server.sync;

            SynthDef(\novelOscRandomizedSafety, {
                |tableBuf, table3DBuf, spectralBuf,
                scaleBuf, freq = 110,
                shape = 0.5, depth = 0.5, position = 0|
                var pm3 = NamedControl.kr(\pm3, 0 ! 9);
                var fm3 = NamedControl.kr(\fm3, 0 ! 9);
                var pm4 = NamedControl.kr(\pm4, 0 ! 16);
                var signals = Array.new;
                var checked;

                signals = signals
                    ++ HarmonicStrideOsc.ar(
                        freq, shape.abs + 0.0001,
                        depth.clip(-0.999, 0.999),
                        normalize: depth
                    )
                    ++ [TableMorph2D.ar(
                        tableBuf, freq, position, depth,
                        tablesX: 2, tablesY: 2,
                        tableSize: 64, mipLevels: 3
                    )]
                    ++ TZSplitOsc.ar(
                        freq, freq.neg,
                        phaseMod: depth * 20,
                        symmetry: shape,
                        drive: depth.abs * 32
                    )
                    ++ ScaleSpreadOsc.ar(
                        scaleBuf, freq,
                        center: position * 64,
                        spread: shape * 16,
                        voices: 8,
                        period: depth.abs * 20,
                        modulation: depth * 32
                    )
                    ++ [SpectralBasisOsc.ar(
                        freq,
                        basis: position * 8,
                        center: position,
                        width: shape,
                        tilt: depth * 16,
                        skew: depth,
                        refresh: depth.abs * 300,
                        tableSize: 128
                    )]
                    ++ PartialClusterOsc.ar(
                        freq,
                        partials: position.abs * 96,
                        spacing: shape,
                        warp: depth * 8,
                        tilt: depth * 16,
                        combPeriod: shape * 5000,
                        combDepth: depth,
                        voices: 3,
                        detune: position.abs * 40,
                        phaseSpread: depth,
                        maxPartials: 128
                    )
                    ++ SyncRingOsc.ar(
                        freq,
                        ratio1: shape * 100,
                        ratio2: depth * 100,
                        amount1: position,
                        amount2: depth,
                        carrierWave: 2,
                        modWave1: 3,
                        modWave2: 1
                    )
                    ++ PMNetworkOsc.ar(
                        freq, [1, shape, depth],
                        pm3, fm3,
                        [1, depth, shape],
                        [0, 2, 3], 2
                    )
                    ++ ClusterPMOsc.ar(
                        freq, [1, shape, depth, position],
                        [1, shape, depth, position],
                        [1, depth, shape, position],
                        pm4, [1, depth, shape, position], 2
                    )
                    ++ ScannedNetworkOsc.ar(
                        freq, depth, shape, position,
                        depth, WhiteNoise.ar(depth),
                        Impulse.ar(position.abs * 2000),
                        depth, position, nodes: 32
                    )
                    ++ [VectorPhaseOsc.ar(
                        freq, [shape, position], [depth, shape],
                        phaseMod: depth * 40,
                        sync: Impulse.ar(position.abs * 3000),
                        oversample: 4
                    )]
                    ++ [SpectralFrameOsc.ar(
                        spectralBuf, freq,
                        frame: position * 20,
                        stretch: shape, tilt: depth * 30,
                        blur: depth, activePartials: position * 16,
                        frames: 2, partials: 4
                    )]
                    ++ RippleFormantOsc.ar(
                        freq, shape * 30000, depth * 30000,
                        position, depth, shape, position,
                        depth, shape, position,
                        oversample: 4
                    )
                    ++ ShiftLogicOsc.ar(
                        freq, freq.neg, shape, depth,
                        position, depth, shape,
                        bits: 16, dacBits: 7, seed: 123
                    )
                    ++ QuadratureFeedbackOsc.ar(
                        freq, WhiteNoise.ar(depth),
                        [shape, depth, position, shape.neg],
                        inject: depth, stability: position,
                        drive: shape, oversample: 8
                    )
                    ++ [SyncModeOsc.ar(
                        freq,
                        Impulse.ar(position.abs * 3000),
                        3, position, shape, 3,
                        depth, 4
                    )]
                    ++ [TableMorph3D.ar(
                        table3DBuf, freq,
                        position, shape, depth,
                        tablesX: 2, tablesY: 2, tablesZ: 2,
                        tableSize: 64, mipLevels: 3
                    )]
                    ++ RatioFamilyOsc.ar(
                        freq, [1, 1.5, 2],
                        spread: shape, skew: position,
                        curve: depth, linearFM: freq.neg
                    )
                    ++ PitchRegisterOsc.ar(
                        Impulse.ar(position.abs * 1000),
                        shape, scaleBuf, freq.abs.max(1),
                        initialDegrees: [0, -2, -4],
                        harmonics: 8, decay: depth,
                        glide: position
                    );

                checked = CheckBadValues.ar(
                    Mix(signals), id: 9100, post: 2
                );
                Out.ar(64, checked * 0)
            }).add;

            SynthDef(\novelOscInvalidBuffers, {
                var signal = TableMorph2D.ar(
                    \badTable.kr,
                    tablesX: 2, tablesY: 2,
                    tableSize: 64, mipLevels: 3
                );
                signal = signal + Mix(
                    ScaleSpreadOsc.ar(
                        \badScale.kr, voices: 4
                    )
                );
                signal = signal + TableMorph3D.ar(
                    \badTable3.kr,
                    tablesX: 2, tablesY: 2, tablesZ: 2,
                    tableSize: 64, mipLevels: 3
                );
                signal = signal + SpectralFrameOsc.ar(
                    \badSpectral.kr,
                    frames: 2, partials: 4
                );
                Out.ar(64, signal * 0)
            }).add;
            server.sync;

            synth = Synth(\novelOscRandomizedSafety, [
                \tableBuf, tableBuffer,
                \table3DBuf, table3DBuffer,
                \spectralBuf, spectralBank.buffer,
                \scaleBuf, scaleBuffer
            ], server);
            invalidSynth = Synth(\novelOscInvalidBuffers, [
                \badTable, 16000000,
                \badScale, 15999999,
                \badTable3, 15999998,
                \badSpectral, 15999997
            ], server);

            80.do { |iteration|
                var frequency = rrand(-300000.0, 300000.0);
                var shape = rrand(-4.0, 4.0);
                var depth = rrand(-4.0, 4.0);
                var position = rrand(-8.0, 8.0);
                var pm3 = Array.fill(9, {
                    rrand(-40.0, 40.0)
                });
                var fm3 = Array.fill(9, {
                    rrand(-200000.0, 200000.0)
                });
                var pm4 = Array.fill(16, {
                    rrand(-40.0, 40.0)
                });

                if(iteration == 20) { frequency = inf };
                synth.set(
                    \freq, frequency,
                    \shape, shape,
                    \depth, depth,
                    \position, position
                );
                synth.setn(\pm3, pm3);
                synth.setn(\fm3, fm3);
                synth.setn(\pm4, pm4);
                0.005.wait;
                if((iteration % 10) == 0) {
                    server.sync
                }
            };

            tableBuffer.free;
            table3DBuffer.free;
            spectralBank.buffer.free;
            scaleBuffer.free;
            server.sync;
            0.1.wait;

            replacement = Buffer.alloc(server, 64 * 12, 1);
            server.sync;
            0.1.wait;

            if(server.serverRunning.not) {
                Error(
                    "Server stopped during randomized safety test."
                ).throw
            };

            [synth, invalidSynth].do(_.free);
            replacement.free;
            server.sync;
            "NOVEL_OSC_RANDOMIZED_SAFETY_OK".postln;
            server.quit;
            0.exit;
        } { |error|
            error.reportError;
            [synth, invalidSynth].do { |node|
                if(node.notNil) { node.free }
            };
            [
                tableBuffer, table3DBuffer,
                if(spectralBank.notNil) {
                    spectralBank.buffer
                } { nil },
                scaleBuffer, replacement
            ].do {
                |buffer|
                if(buffer.notNil) { buffer.free }
            };
            server.quit;
            1.exit;
        }
    }).play(SystemClock)
}, 15, {
    "NovelOscUGens randomized safety server failed to boot.".error;
    1.exit;
});

server.boot;
)
