(
var sampleRates = [48000, 96000];
var port = 57133;

Routine({
    try {
        sampleRates.do { |requestedRate|
            var server = Server(
                ("novelOscBench" ++ requestedRate).asSymbol,
                NetAddr("127.0.0.1", port)
            );
            var booted = FlowVar.new;

            server.options.numOutputBusChannels = 2;
            server.options.numInputBusChannels = 0;
            server.options.numAudioBusChannels = 256;
            server.options.blockSize = 64;
            server.options.sampleRate = requestedRate;
            server.options.memSize = 262144;
            server.options.numWireBufs = 4096;
            server.waitForBoot({
                booted.value = true
            }, 20, {
                booted.value = false
            });
            server.boot;
            if(booted.value.not) {
                Error(
                    "Benchmark server failed to boot at % Hz."
                    .format(requestedRate)
                ).throw
            };

            {
                var size = 256;
                var tableBuffer;
                var scaleBuffer;
                var cases;
                var baselineSamples;
                var baseline;
                var buildTZ;
                var buildSync;
                var buildPM;
                var scaledCount;

                tableBuffer = TableMorph2D.prepareBuffer(
                    server,
                    16.collect { |index|
                        Signal.sineFill(
                            size,
                            [1, index / 24, index / 48]
                        )
                    },
                    4, 4, 5
                );
                scaleBuffer = ScaleSpreadOsc.prepareScale(
                    server,
                    [1, 9/8, 5/4, 4/3, 3/2, 5/3, 15/8]
                );
                server.sync;

                SynthDef(\novelBenchHarmonic, {
                    Out.ar(
                        64,
                        Mix(HarmonicStrideOsc.ar(
                            41, 1.05, 0.96,
                            maxPartials: 1024
                        )) * 1e-9
                    )
                }).add;
                SynthDef(\novelBenchTable, {
                    |tableBuf|
                    Out.ar(
                        64,
                        TableMorph2D.ar(
                            tableBuf, 137,
                            SinOsc.ar(17).range(0, 1),
                            SinOsc.ar(23).range(0, 1),
                            tablesX: 4, tablesY: 4,
                            tableSize: 256, mipLevels: 5
                        ) * 1e-9
                    )
                }).add;
                SynthDef(\novelBenchScale8, {
                    |scaleBuf|
                    Out.ar(
                        64,
                        Mix(ScaleSpreadOsc.ar(
                            scaleBuf, 61,
                            center: SinOsc.ar(7, 0, 6),
                            spread: SinOsc.ar(9).range(0.2, 2),
                            voices: 8, modulation: 0.2
                        )) * 1e-9
                    )
                }).add;
                SynthDef(\novelBenchScale24, {
                    |scaleBuf|
                    Out.ar(
                        64,
                        Mix(ScaleSpreadOsc.ar(
                            scaleBuf, 61,
                            center: SinOsc.ar(7, 0, 6),
                            spread: SinOsc.ar(9).range(0.2, 2),
                            voices: 24, modulation: 0.2
                        )) * 1e-9
                    )
                }).add;
                SynthDef(\novelBenchSpectral128, {
                    Out.ar(
                        64,
                        SpectralBasisOsc.ar(
                            113, 3,
                            LFTri.kr(8).range(0.1, 0.9),
                            0.17, refresh: 30,
                            tableSize: 128
                        ) * 1e-9
                    )
                }).add;
                SynthDef(\novelBenchSpectral2048, {
                    Out.ar(
                        64,
                        SpectralBasisOsc.ar(
                            113, 3,
                            LFTri.kr(8).range(0.1, 0.9),
                            0.17, refresh: 30,
                            tableSize: 2048
                        ) * 1e-9
                    )
                }).add;
                SynthDef(\novelBenchPartial64x1, {
                    Out.ar(
                        64,
                        Mix(PartialClusterOsc.ar(
                            43, 64, voices: 1,
                            maxPartials: 64
                        )) * 1e-9
                    )
                }).add;
                SynthDef(\novelBenchPartial256x5, {
                    Out.ar(
                        64,
                        Mix(PartialClusterOsc.ar(
                            43, 256, warp: -0.08,
                            voices: 5, detune: 0.1,
                            maxPartials: 256
                        )) * 1e-9
                    )
                }).add;

                buildTZ = { |name, oversample|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            Mix(TZSplitOsc.ar(
                                83,
                                linearFM: SinOsc.ar(
                                    53, 0, 100
                                ),
                                symmetry: 0.31, drive: 6,
                                oversample: oversample
                            )) * 1e-9
                        )
                    }).add
                };
                buildSync = { |name, oversample|
                    SynthDef(name, {
                        Out.ar(
                            64,
                            Mix(SyncRingOsc.ar(
                                79, 2.5, 3.25, 0.8, 0.6,
                                carrierWave: 2,
                                modWave1: 3, modWave2: 1,
                                oversample: oversample
                            )) * 1e-9
                        )
                    }).add
                };
                buildPM = { |name, oversample, audioRate|
                    SynthDef(name, {
                        var modulation = if(audioRate) {
                            SinOsc.ar(211, 0, 1.5)
                        } {
                            1.5
                        };
                        Out.ar(
                            64,
                            Mix(PMNetworkOsc.ar(
                                71, [1, 1.5, 2],
                                [
                                    0, modulation, 0,
                                    0, 0, 0.7,
                                    0.3, 0, 0
                                ],
                                0 ! 9,
                                [1, 0.7, 0.5],
                                [0, 2, 3],
                                oversample
                            )) * 1e-9
                        )
                    }).add
                };
                [
                    [\novelBenchTZ1, 1],
                    [\novelBenchTZ2, 2],
                    [\novelBenchTZ4, 4]
                ].do { |item| buildTZ.(*item) };
                [
                    [\novelBenchSync1, 1],
                    [\novelBenchSync4, 4]
                ].do { |item| buildSync.(*item) };
                buildPM.(\novelBenchPM1Static, 1, false);
                buildPM.(\novelBenchPM4Static, 4, false);
                buildPM.(\novelBenchPM4Audio, 4, true);
                SynthDef(\novelBenchCluster4, {
                    Out.ar(
                        64,
                        Mix(ClusterPMOsc.ar(
                            53,
                            pm: [
                                0, 0.4, 0, 0,
                                0, 0, 0.3, 0,
                                0.2, 0, 0, 0,
                                0, 0.1, 0, 0
                            ],
                            oversample: 4
                        )) * 1e-9
                    )
                }).add;
                server.sync;

                scaledCount = { |baseCount|
                    (
                        baseCount * 48000
                        / server.actualSampleRate
                    ).floor.asInteger.max(1)
                };
                cases = [
                    [\novelBenchHarmonic,
                        scaledCount.(48), #[]],
                    [\novelBenchTable,
                        scaledCount.(48),
                        [\tableBuf, tableBuffer]],
                    [\novelBenchTZ1,
                        scaledCount.(64), #[]],
                    [\novelBenchTZ2,
                        scaledCount.(48), #[]],
                    [\novelBenchTZ4,
                        scaledCount.(32), #[]],
                    [\novelBenchScale8,
                        scaledCount.(16),
                        [\scaleBuf, scaleBuffer]],
                    [\novelBenchScale24,
                        scaledCount.(4),
                        [\scaleBuf, scaleBuffer]],
                    [\novelBenchSpectral128,
                        scaledCount.(24), #[]],
                    [\novelBenchSpectral2048,
                        scaledCount.(4), #[]],
                    [\novelBenchPartial64x1,
                        scaledCount.(8), #[]],
                    [\novelBenchPartial256x5,
                        scaledCount.(2), #[]],
                    [\novelBenchSync1,
                        scaledCount.(64), #[]],
                    [\novelBenchSync4,
                        scaledCount.(16), #[]],
                    [\novelBenchPM1Static,
                        scaledCount.(48), #[]],
                    [\novelBenchPM4Static,
                        scaledCount.(16), #[]],
                    [\novelBenchPM4Audio,
                        scaledCount.(16), #[]],
                    [\novelBenchCluster4,
                        scaledCount.(4), #[]]
                ];

                1.0.wait;
                baselineSamples = Array.fill(6, {
                    0.2.wait;
                    server.avgCPU
                });
                baseline = baselineSamples.mean;
                "NOVEL_OSC_BENCHMARK % Hz baseline=%"
                .format(
                    server.actualSampleRate,
                    baseline.round(0.001)
                ).postln;
                "case instances avgCPU peakCPU incrementalAvgPerInstance"
                .postln;

                cases.do { |item|
                    var name = item[0];
                    var count = item[1];
                    var arguments = item[2];
                    var group = Group(server);
                    var averageSamples;
                    var peakSamples;
                    var average;
                    var peak;

                    count.do {
                        Synth(name, arguments, group)
                    };
                    1.0.wait;
                    averageSamples = Array.fill(6, {
                        0.2.wait;
                        server.avgCPU
                    });
                    peakSamples = Array.fill(6, {
                        0.2.wait;
                        server.peakCPU
                    });
                    average = averageSamples.mean;
                    peak = peakSamples.maxItem;
                    [
                        name, count,
                        average.round(0.001),
                        peak.round(0.001),
                        (
                            (average - baseline)
                            / count
                        ).round(0.0001)
                    ].postln;
                    group.free;
                    server.sync;
                    0.5.wait
                };

                [tableBuffer, scaleBuffer].do(_.free);
                server.sync;
            }.value;

            server.quit;
            1.0.wait
        };
        "NOVEL_OSC_BENCHMARK_OK".postln;
        0.exit;
    } { |error|
        error.reportError;
        Server.killAll;
        1.exit;
    }
}).play(SystemClock);
)
