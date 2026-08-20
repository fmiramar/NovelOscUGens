(
var sampleRates = [48000, 96000];
var port = 57144;

Routine({
    try {
        sampleRates.do { |requestedRate|
            var server = Server(
                ("novelOscAdditionalBench" ++ requestedRate).asSymbol,
                NetAddr("127.0.0.1", port)
            );
            var booted = FlowVar.new;

            server.options.numOutputBusChannels = 2;
            server.options.numInputBusChannels = 0;
            server.options.numAudioBusChannels = 256;
            server.options.blockSize = 64;
            server.options.sampleRate = requestedRate;
            server.options.memSize = 524288;
            server.options.numWireBufs = 8192;
            server.waitForBoot({
                booted.value = true
            }, 20, {
                booted.value = false
            });
            server.boot;
            if(booted.value.not) {
                Error(
                    "Additional benchmark server failed to boot at % Hz."
                    .format(requestedRate)
                ).throw
            };

            {
                var size = 256;
                var tableBuffer;
                var spectralBank;
                var scaleBuffer;
                var cases;
                var baseline;
                var scaledCount;
                var makeScanned;
                var makeVector;
                var makeSpectral;
                var makeRipple;
                var makeQuadrature;
                var makeSync;
                var makeRatio;
                var makePitch;

                tableBuffer = TableMorph3D.prepareBuffer(
                    server,
                    64.collect { |index|
                        Signal.sineFill(
                            size,
                            [1, index / 96, index / 192]
                        )
                    },
                    4, 4, 4, 5
                );
                SpectralFrameBuffer.prepare(
                    server,
                    4.collect { |frame|
                        [
                            Array.series(512, 1, 1 + (frame * 0.001)),
                            Array.fill(512, { |partial|
                                (partial + 1).reciprocal
                            })
                        ]
                    },
                    action: { |object| spectralBank = object }
                );
                scaleBuffer = Buffer.loadCollection(
                    server, [0, 2, 3, 5, 7, 8, 10]
                );
                server.sync;

                makeScanned = { |name, nodes|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            Mix(ScannedNetworkOsc.ar(
                                73, 0.35, 0.45, 0.01, 0.015,
                                PinkNoise.ar(0.002),
                                Impulse.ar(11), 0.1, 0.4,
                                nodes: nodes
                            )) * 1e-9
                        )
                    }).add
                };
                makeVector = { |name, oversample|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            VectorPhaseOsc.ar(
                                97,
                                [0.08, 0.38, 0.72],
                                [0.3, 0.58, 0.94],
                                phaseMod: SinOsc.ar(19, 0, 0.2),
                                oversample: oversample
                            ) * 1e-9
                        )
                    }).add
                };
                makeSpectral = { |name, partials|
                    SynthDef(name, {
                        |spectralBuf|
                        Out.ar(
                            64,
                            SpectralFrameOsc.ar(
                                spectralBuf, 41,
                                frame: LFTri.kr(7).range(0, 3),
                                stretch: 0.9, tilt: -1,
                                blur: 0.2,
                                activePartials: partials,
                                frames: 4, partials: 512
                            ) * 1e-9
                        )
                    }).add
                };
                makeRipple = { |name, oversample|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            Mix(RippleFormantOsc.ar(
                                67, 1700, 4300,
                                0.008, 0.004, 1.2, -0.8,
                                oversample: oversample
                            )) * 1e-9
                        )
                    }).add
                };
                makeQuadrature = { |name, oversample|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            Mix(QuadratureFeedbackOsc.ar(
                                83, Saw.ar(31, 0.05),
                                [0.8, -0.6, 0.4, -0.3],
                                inject: 0.5, stability: 1.5,
                                drive: 4, oversample: oversample
                            )) * 1e-9
                        )
                    }).add
                };
                makeSync = { |name, oversample|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            SyncModeOsc.ar(
                                91, Impulse.ar(137), 0,
                                resetPhase: 0.2, shape: 2,
                                oversample: oversample
                            ) * 1e-9
                        )
                    }).add
                };
                makeRatio = { |name, voices|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            Mix(RatioFamilyOsc.ar(
                                47,
                                Array.series(voices, 1, 0.125),
                                spread: SinOsc.kr(3).range(0.5, 1.5),
                                skew: 0.37, curve: 0.3
                            )) * 1e-9
                        )
                    }).add
                };
                makePitch = { |name, voices|
                    SynthDef(name, {
                        |scaleBuf|
                        var trig = Impulse.ar(13);
                        Out.ar(
                            64,
                            Mix(PitchRegisterOsc.ar(
                                trig,
                                Demand.ar(
                                    trig, 0,
                                    Dseries(0, 1, inf)
                                ),
                                scaleBuf, 41,
                                initialDegrees:
                                    Array.series(voices, 0, -1),
                                harmonics: 32
                            )) * 1e-9
                        )
                    }).add
                };

                makeScanned.(\novelBenchScanned32, 32);
                makeScanned.(\novelBenchScanned128, 128);
                makeScanned.(\novelBenchScanned512, 512);
                makeVector.(\novelBenchVector1, 1);
                makeVector.(\novelBenchVector4, 4);
                makeSpectral.(\novelBenchFrames16, 16);
                makeSpectral.(\novelBenchFrames128, 128);
                makeSpectral.(\novelBenchFrames512, 512);
                makeRipple.(\novelBenchRipple1, 1);
                makeRipple.(\novelBenchRipple4, 4);
                SynthDef(\novelBenchShiftLogic, {
                    Out.ar(
                        64,
                        Mix(ShiftLogicOsc.ar(
                            79, 103, 0.8, -0.6,
                            change: 0.7, bits: 24,
                            dacBits: 8, seed: 1234
                        )) * 1e-9
                    )
                }).add;
                makeQuadrature.(\novelBenchQuadrature1, 1);
                makeQuadrature.(\novelBenchQuadrature4, 4);
                makeQuadrature.(\novelBenchQuadrature8, 8);
                makeSync.(\novelBenchSyncMode1, 1);
                makeSync.(\novelBenchSyncMode4, 4);
                SynthDef(\novelBenchTable3D, {
                    |tableBuf|
                    Out.ar(
                        64,
                        TableMorph3D.ar(
                            tableBuf, 113,
                            SinOsc.ar(13).range(0, 1),
                            SinOsc.ar(17).range(0, 1),
                            SinOsc.ar(19).range(0, 1),
                            tablesX: 4, tablesY: 4, tablesZ: 4,
                            tableSize: 256, mipLevels: 5
                        ) * 1e-9
                    )
                }).add;
                makeRatio.(\novelBenchRatio6, 6);
                makeRatio.(\novelBenchRatio24, 24);
                makePitch.(\novelBenchPitch3, 3);
                makePitch.(\novelBenchPitch12, 12);
                server.sync;

                scaledCount = { |baseCount|
                    (
                        baseCount * 48000
                        / requestedRate
                    ).floor.asInteger.max(1)
                };
                cases = [
                    [\novelBenchScanned32, scaledCount.(32), #[]],
                    [\novelBenchScanned128, scaledCount.(16), #[]],
                    [\novelBenchScanned512, scaledCount.(4), #[]],
                    [\novelBenchVector1, scaledCount.(64), #[]],
                    [\novelBenchVector4, scaledCount.(24), #[]],
                    [\novelBenchFrames16, scaledCount.(32),
                        [\spectralBuf, spectralBank.buffer]],
                    [\novelBenchFrames128, scaledCount.(8),
                        [\spectralBuf, spectralBank.buffer]],
                    [\novelBenchFrames512, scaledCount.(2),
                        [\spectralBuf, spectralBank.buffer]],
                    [\novelBenchRipple1, scaledCount.(48), #[]],
                    [\novelBenchRipple4, scaledCount.(16), #[]],
                    [\novelBenchShiftLogic, scaledCount.(48), #[]],
                    [\novelBenchQuadrature1, scaledCount.(48), #[]],
                    [\novelBenchQuadrature4, scaledCount.(16), #[]],
                    [\novelBenchQuadrature8, scaledCount.(8), #[]],
                    [\novelBenchSyncMode1, scaledCount.(64), #[]],
                    [\novelBenchSyncMode4, scaledCount.(24), #[]],
                    [\novelBenchTable3D, scaledCount.(32),
                        [\tableBuf, tableBuffer]],
                    [\novelBenchRatio6, scaledCount.(16), #[]],
                    [\novelBenchRatio24, scaledCount.(4), #[]],
                    [\novelBenchPitch3, scaledCount.(12),
                        [\scaleBuf, scaleBuffer]],
                    [\novelBenchPitch12, scaledCount.(3),
                        [\scaleBuf, scaleBuffer]]
                ];

                0.75.wait;
                baseline = Array.fill(4, {
                    0.15.wait;
                    server.avgCPU
                }).mean;
                "NOVEL_OSC_ADDITIONAL_BENCHMARK % Hz baseline=%"
                .format(
                    requestedRate,
                    baseline.round(0.001)
                ).postln;
                "case instances avgCPU peakCPU incrementalAvgPerInstance"
                .postln;

                cases.do { |item|
                    var name = item[0];
                    var count = item[1];
                    var arguments = item[2];
                    var group = Group(server);
                    var average;
                    var peak;

                    count.do {
                        Synth(name, arguments, group)
                    };
                    0.6.wait;
                    average = Array.fill(4, {
                        0.15.wait;
                        server.avgCPU
                    }).mean;
                    peak = Array.fill(4, {
                        0.15.wait;
                        server.peakCPU
                    }).maxItem;
                    [
                        name, count,
                        average.round(0.001),
                        peak.round(0.001),
                        (
                            (average - baseline) / count
                        ).round(0.0001)
                    ].postln;
                    group.free;
                    server.sync;
                    0.25.wait
                };

                [
                    tableBuffer, spectralBank.buffer, scaleBuffer
                ].do(_.free);
                server.sync;
            }.value;

            server.quit;
            0.75.wait
        };
        "NOVEL_OSC_ADDITIONAL_BENCHMARK_OK".postln;
        0.exit;
    } { |error|
        error.reportError;
        Server.killAll;
        1.exit;
    }
}).play(SystemClock);
)
