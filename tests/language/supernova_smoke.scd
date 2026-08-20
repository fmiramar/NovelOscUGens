(
var server = Server(
    \novelOscSupernovaSmoke,
    NetAddr("127.0.0.1", 57135)
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
        var scaleBuffer;
        var recordBuffer;
        var completed;
        var data;

        try {
            tableBuffer = TableMorph2D.prepareBuffer(
                server,
                [
                    Signal.sineFill(size, [1]),
                    Signal.sineFill(size, [1, 0.2]),
                    Signal.sineFill(size, [0, 1]),
                    Signal.sineFill(size, [1, 0, 0.2])
                ],
                2, 2, 3
            );
            scaleBuffer = ScaleSpreadOsc.prepareScale(
                server, [1, 9/8, 5/4, 4/3, 3/2]
            );
            recordBuffer = Buffer.alloc(server, 2048, 1);
            server.sync;

            SynthDef(\novelOscSupernovaSmoke, {
                |tableBuf, scaleBuf, recordBuf|
                var signal = Mix(
                    HarmonicStrideOsc.ar(101)
                    ++ [TableMorph2D.ar(
                        tableBuf, 137,
                        tablesX: 2, tablesY: 2,
                        tableSize: 64, mipLevels: 3
                    )]
                    ++ TZSplitOsc.ar(89)
                    ++ ScaleSpreadOsc.ar(
                        scaleBuf, 67, voices: 5
                    )
                    ++ [SpectralBasisOsc.ar(
                        113, tableSize: 128
                    )]
                    ++ PartialClusterOsc.ar(
                        47, 32, voices: 2,
                        maxPartials: 64
                    )
                    ++ SyncRingOsc.ar(73)
                    ++ PMNetworkOsc.ar(59)
                    ++ ClusterPMOsc.ar(43)
                ) * 0.02;
                RecordBuf.ar(
                    signal, recordBuf,
                    loop: 0, doneAction: Done.freeSelf
                )
            }).add;
            server.sync;

            Synth(\novelOscSupernovaSmoke, [
                \tableBuf, tableBuffer,
                \scaleBuf, scaleBuffer,
                \recordBuf, recordBuffer
            ], server);
            0.2.wait;
            server.sync;

            completed = FlowVar.new;
            recordBuffer.loadToFloatArray(
                action: { |array|
                    data = array;
                    completed.value = true
                }
            );
            completed.value;
            if(data.any { |value|
                value.isNaN or: { value.abs >= inf }
            }) {
                Error(
                    "supernova smoke emitted NaN or infinity."
                ).throw
            };
            if(data.abs.maxItem < 1e-5) {
                Error(
                    "supernova smoke remained silent."
                ).throw
            };

            "NOVEL_OSC_SUPERNOVA_SMOKE_OK peak=%"
            .format(data.abs.maxItem.round(0.0001))
            .postln;
            [recordBuffer, tableBuffer, scaleBuffer].do(_.free);
            server.sync;
            server.quit;
            Server.scsynth;
            0.exit;
        } { |error|
            error.reportError;
            [recordBuffer, tableBuffer, scaleBuffer].do {
                |buffer|
                if(buffer.notNil) { buffer.free }
            };
            server.quit;
            Server.scsynth;
            1.exit;
        }
    }).play(SystemClock)
}, 20, {
    "NovelOscUGens supernova smoke server failed to boot.".error;
    Server.scsynth;
    1.exit;
});

server.boot;
)
