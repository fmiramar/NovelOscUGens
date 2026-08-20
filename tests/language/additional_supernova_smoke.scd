(
var server = Server(
    \novelOscAdditionalSupernova,
    NetAddr("127.0.0.1", 57141)
);

Server.supernova;
server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 256;
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
        var data;
        var completed;

        try {
            tableBuffer = TableMorph3D.prepareBuffer(
                server,
                8.collect { |index|
                    Signal.sineFill(size, [1, index / 20])
                },
                2, 2, 2, 3
            );
            SpectralFrameBuffer.prepare(
                server,
                [
                    [[1, 2, 3], [1, 0.5, 0.2]],
                    [[1, 1.7, 3.5], [1, 0.4, 0.15]]
                ],
                action: { |object| spectralBank = object }
            );
            scaleBuffer = Buffer.loadCollection(
                server, [0, 2, 3, 5, 7, 8, 10]
            );
            recordBuffer = Buffer.alloc(server, 4096, 1);
            server.sync;

            SynthDef(\novelOscAdditionalSupernova, {
                |recordBuf, tableBuf, spectralBuf, scaleBuf|
                var trig = Impulse.ar(9);
                var mix = Mix(
                    ScannedNetworkOsc.ar(
                        61, nodes: 32, strike: trig
                    )
                    ++ [VectorPhaseOsc.ar(
                        73, [0.3], [0.7]
                    )]
                    ++ [SpectralFrameOsc.ar(
                        spectralBuf, 59,
                        frames: 2, partials: 3
                    )]
                    ++ RippleFormantOsc.ar(47, 500, 1400)
                    ++ ShiftLogicOsc.ar(53, 71, seed: 87)
                    ++ QuadratureFeedbackOsc.ar(
                        67, feedback: [0.2, -0.2, 0.1, -0.1]
                    )
                    ++ [SyncModeOsc.ar(79, trig, 1, shape: 1)]
                    ++ [TableMorph3D.ar(
                        tableBuf, 83, 0.2, 0.5, 0.8,
                        tablesX: 2, tablesY: 2, tablesZ: 2,
                        tableSize: 64, mipLevels: 3
                    )]
                    ++ RatioFamilyOsc.ar(43, [1, 1.5, 2])
                    ++ PitchRegisterOsc.ar(
                        trig, 3, scaleBuf, 41,
                        initialDegrees: [0, -2, -4],
                        harmonics: 8
                    )
                ) * 0.002;
                RecordBuf.ar(
                    mix, recordBuf,
                    loop: 0, doneAction: Done.freeSelf
                )
            }).add;
            server.sync;
            Synth(\novelOscAdditionalSupernova, [
                \recordBuf, recordBuffer,
                \tableBuf, tableBuffer,
                \spectralBuf, spectralBank.buffer,
                \scaleBuf, scaleBuffer
            ], server);
            0.25.wait;
            server.sync;

            completed = Condition({ data.notNil });
            recordBuffer.loadToFloatArray(
                action: { |array|
                    data = array;
                    completed.signal
                }
            );
            completed.wait;
            if(data.any { |value|
                value.isNaN or: { value.abs >= inf }
            }) {
                Error(
                    "Additional supernova smoke emitted NaN or infinity."
                ).throw
            };
            if(data.abs.maxItem < 1e-5) {
                Error(
                    "Additional supernova smoke remained silent."
                ).throw
            };
            "NOVEL_OSC_ADDITIONAL_SUPERNOVA_OK peak=%"
            .format(data.abs.maxItem.round(0.0001)).postln;
            [
                recordBuffer, tableBuffer,
                spectralBank.buffer, scaleBuffer
            ].do(_.free);
            server.sync;
            server.quit;
            Server.scsynth;
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
            Server.scsynth;
            1.exit;
        }
    }).play(SystemClock)
}, 20, {
    "NovelOscUGens additional supernova server failed to boot.".error;
    Server.scsynth;
    1.exit;
});

server.boot;
)
