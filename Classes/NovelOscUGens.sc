HarmonicStrideOsc : MultiOutUGen {
    *ar {
        |freq = 440, stride = 1, decay = 0.5, phase = 0,
        normalize = 1, maxPartials = 2048,
        mul = 1, add = 0|
        if(maxPartials.isInteger.not
            or: { maxPartials < 1 }
            or: { maxPartials > 16384 }) {
            Error(
                "HarmonicStrideOsc: maxPartials must be a literal integer from 1 to 16384."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 2, freq, stride, decay, phase,
                normalize, maxPartials]
        ).collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "HarmonicStrideOsc is audio-rate only; use HarmonicStrideOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

TableMorph2D : UGen {
    *ar {
        |bufnum, freq = 440, x = 0, y = 0, phase = 0,
        tablesX = 4, tablesY = 4, tableSize = 2048,
        mipLevels = 1, interpolation = 2,
        mul = 1, add = 0|
        [tablesX, tablesY, tableSize, mipLevels, interpolation]
        .do { |value, index|
            if(value.isInteger.not) {
                Error(
                    "TableMorph2D: layout and interpolation arguments must be literal integers (argument index %)."
                    .format(index)
                ).throw
            }
        };
        if(tablesX < 1 or: { tablesX > 1024 }
            or: { tablesY < 1 }
            or: { tablesY > 1024 }) {
            Error(
                "TableMorph2D: tablesX and tablesY must be from 1 to 1024."
            ).throw
        };
        if(tableSize < 64
            or: { tableSize > 65536 }
            or: { tableSize.isPowerOfTwo.not }) {
            Error(
                "TableMorph2D: tableSize must be a power of two from 64 to 65536."
            ).throw
        };
        if(mipLevels < 1 or: { mipLevels > 16 }) {
            Error(
                "TableMorph2D: mipLevels must be from 1 to 16."
            ).throw
        };
        if(interpolation < 0 or: { interpolation > 2 }) {
            Error(
                "TableMorph2D: interpolation must be 0, 1, or 2."
            ).throw
        };
        ^this.multiNewList(
            [\audio, bufnum, freq, x, y, phase,
                tablesX, tablesY, tableSize,
                mipLevels, interpolation]
        ).madd(mul, add)
    }

    *kr {
        Error(
            "TableMorph2D is audio-rate only; use TableMorph2D.ar."
        ).throw
    }

    *prepareBuffer {
        |server, tables, tablesX, tablesY, mipLevels = 8,
        normalize = true, action|
        var sourceTables, tableSize, expectedTables;
        var expectedSamples;
        var processed, cosineTable, flattened, position;
        server = server ? Server.default;
        if(tablesX.isInteger.not
            or: { tablesY.isInteger.not }
            or: { tablesX < 1 }
            or: { tablesX > 1024 }
            or: { tablesY < 1 }
            or: { tablesY > 1024 }) {
            Error(
                "TableMorph2D.prepareBuffer: grid dimensions must be literal integers from 1 to 1024."
            ).throw
        };
        if(mipLevels.isInteger.not
            or: { mipLevels < 1 }
            or: { mipLevels > 16 }) {
            Error(
                "TableMorph2D.prepareBuffer: mipLevels must be a literal integer from 1 to 16."
            ).throw
        };
        sourceTables = tables.asArray.collect {
            |table| table.asArray
        };
        expectedTables = tablesX * tablesY;
        if(sourceTables.size != expectedTables) {
            Error(
                "TableMorph2D.prepareBuffer: expected % tables, received %."
                .format(expectedTables, sourceTables.size)
            ).throw
        };
        if(sourceTables.isEmpty
            or: { sourceTables[0].isEmpty }) {
            Error(
                "TableMorph2D.prepareBuffer: tables must not be empty."
            ).throw
        };
        tableSize = sourceTables[0].size;
        if(tableSize < 64
            or: { tableSize > 65536 }
            or: { tableSize.isPowerOfTwo.not }) {
            Error(
                "TableMorph2D.prepareBuffer: table size must be a power of two from 64 to 65536."
            ).throw
        };
        if(sourceTables.any { |table|
            table.size != tableSize
        }) {
            Error(
                "TableMorph2D.prepareBuffer: every table must have the same size."
            ).throw
        };
        expectedSamples =
            expectedTables * tableSize * mipLevels;
        if(expectedSamples > 2147483647) {
            Error(
                "TableMorph2D.prepareBuffer: the declared layout exceeds the server Buffer sample limit."
            ).throw
        };
        if(sourceTables.any { |table|
            table.any { |sample|
                sample.isNumber.not
                or: { sample.isNaN }
                or: { sample.abs >= inf }
            }
        }) {
            Error(
                "TableMorph2D.prepareBuffer: every table sample must be finite."
            ).throw
        };

        processed = sourceTables.collect { |table|
            var signal = Signal.newFrom(table);
            var mean = signal.sum / tableSize;
            var peak;
            signal = signal - mean;
            peak = signal.abs.maxItem;
            if(normalize and: { peak > 0 }) {
                signal = signal / peak
            };
            signal
        };
        cosineTable = Signal.fftCosTable(tableSize);
        flattened = FloatArray.newClear(
            mipLevels * expectedTables * tableSize
        );
        position = 0;
        mipLevels.do { |level|
            processed.do { |table|
                var mip = if(level == 0) {
                    table
                } {
                    this.makeMip(table, level, cosineTable)
                };
                mip.do { |sample|
                    flattened[position] = sample;
                    position = position + 1
                }
            }
        };
        ^Buffer.loadCollection(
            server, flattened, 1, action: action
        )
    }

    *makeMip { |signal, level, cosineTable|
        var real = signal.copy.as(Signal);
        var imaginary = Signal.newClear(signal.size);
        var spectrum = real.fft(imaginary, cosineTable);
        var cutoff = (
            signal.size / (2 ** (level + 1))
        ).asInteger.max(1);
        ((cutoff + 1)..(signal.size - cutoff - 1)).do {
            |bin|
            spectrum.real[bin] = 0;
            spectrum.imag[bin] = 0
        };
        ^spectrum.real.ifft(
            spectrum.imag, cosineTable
        ).real.as(Signal)
    }

    checkInputs { ^this.checkValidInputs }
}

TZSplitOsc : MultiOutUGen {
    *ar {
        |freq = 440, linearFM = 0, phaseMod = 0,
        symmetry = 0.5, drive = 1, reset = 0, flip = 0,
        oversample = 2, mul = 1, add = 0|
        if([1, 2, 4].includes(oversample).not) {
            Error(
                "TZSplitOsc: oversample must be the literal value 1, 2, or 4."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 4, freq, linearFM, phaseMod,
                symmetry, drive, reset, flip, oversample]
        ).collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "TZSplitOsc is audio-rate only; use TZSplitOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

ScaleSpreadOsc : MultiOutUGen {
    *ar {
        |bufnum, freq = 110, center = 0, spread = 1,
        voices = 8, period = 2, modulation = 0,
        stereo = 1, waveform = 0, mul = 1, add = 0|
        if(voices.isInteger.not
            or: { voices < 1 }
            or: { voices > 32 }) {
            Error(
                "ScaleSpreadOsc: voices must be a literal integer from 1 to 32."
            ).throw
        };
        if(waveform.isInteger.not
            or: { waveform < 0 }
            or: { waveform > 2 }) {
            Error(
                "ScaleSpreadOsc: waveform must be a literal integer from 0 to 2."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 2, bufnum, freq, center, spread,
                voices, period, modulation, stereo, waveform]
        ).collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "ScaleSpreadOsc is audio-rate only; use ScaleSpreadOsc.ar."
        ).throw
    }

    *prepareScale { |server, ratios, action|
        var values;
        server = server ? Server.default;
        values = ratios.asArray.flat.collect(_.asFloat);
        if(values.size < 2) {
            Error(
                "ScaleSpreadOsc.prepareScale: provide at least two ratios."
            ).throw
        };
        if((values[0] - 1).abs > 1e-6) {
            Error(
                "ScaleSpreadOsc.prepareScale: the first ratio must be 1."
            ).throw
        };
        values.do { |value, index|
            if(value <= 0
                or: { value.isNaN }
                or: { value.abs >= inf }) {
                Error(
                    "ScaleSpreadOsc.prepareScale: every ratio must be positive and finite."
                ).throw
            };
            if(index > 0
                and: { value <= values[index - 1] }) {
                Error(
                    "ScaleSpreadOsc.prepareScale: ratios must be strictly ascending."
                ).throw
            }
        };
        ^Buffer.loadCollection(
            server, values.as(FloatArray), 1,
            action: action
        )
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

SpectralBasisOsc : UGen {
    *ar {
        |freq = 440, basis = 0, center = 0.25,
        width = 0.15, tilt = 0, skew = 0, phase = 0,
        refresh = 30, tableSize = 1024,
        mul = 1, add = 0|
        if(tableSize.isInteger.not
            or: { tableSize < 128 }
            or: { tableSize > 4096 }
            or: { tableSize.isPowerOfTwo.not }) {
            Error(
                "SpectralBasisOsc: tableSize must be a literal power of two from 128 to 4096."
            ).throw
        };
        ^this.multiNewList(
            [\audio, freq, basis, center, width, tilt,
                skew, phase, refresh, tableSize]
        ).madd(mul, add)
    }

    *kr {
        Error(
            "SpectralBasisOsc is audio-rate only; use SpectralBasisOsc.ar."
        ).throw
    }

    checkInputs { ^this.checkValidInputs }
}

PartialClusterOsc : MultiOutUGen {
    *ar {
        |freq = 110, partials = 128, spacing = 1, warp = 0,
        tilt = 1, combPeriod = 1, combDepth = 0,
        voices = 1, detune = 0, phaseSpread = 0,
        maxPartials = 2048, mul = 1, add = 0|
        if(voices.isInteger.not
            or: { voices < 1 }
            or: { voices > 5 }) {
            Error(
                "PartialClusterOsc: voices must be a literal integer from 1 to 5."
            ).throw
        };
        if(maxPartials.isInteger.not
            or: { maxPartials < 1 }
            or: { maxPartials > 4096 }) {
            Error(
                "PartialClusterOsc: maxPartials must be a literal integer from 1 to 4096."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 2, freq, partials, spacing, warp,
                tilt, combPeriod, combDepth, voices,
                detune, phaseSpread, maxPartials]
        ).collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "PartialClusterOsc is audio-rate only; use PartialClusterOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

SyncRingOsc : MultiOutUGen {
    *ar {
        |freq = 440, ratio1 = 2, ratio2 = 3,
        amount1 = 0.5, amount2 = 0, phase1 = 0, phase2 = 0,
        carrierWave = 0, modWave1 = 0, modWave2 = 0,
        oversample = 2, mul = 1, add = 0|
        [carrierWave, modWave1, modWave2].do { |waveform|
            if(waveform.isInteger.not
                or: { waveform < 0 }
                or: { waveform > 3 }) {
                Error(
                    "SyncRingOsc: waveform values must be literal integers from 0 to 3."
                ).throw
            }
        };
        if([1, 2, 4].includes(oversample).not) {
            Error(
                "SyncRingOsc: oversample must be the literal value 1, 2, or 4."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 4, freq, ratio1, ratio2,
                amount1, amount2, phase1, phase2,
                carrierWave, modWave1, modWave2, oversample]
        ).collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "SyncRingOsc is audio-rate only; use SyncRingOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

PMNetworkOsc : MultiOutUGen {
    *ar {
        |freq = 110, ratios = #[1, 2, 3],
        pm = #[0, 0, 0, 0, 0, 0, 0, 0, 0],
        fm = #[0, 0, 0, 0, 0, 0, 0, 0, 0],
        amplitudes = #[1, 1, 1],
        waveforms = #[0, 0, 0], oversample = 2,
        mul = 1, add = 0|
        var ratioArray = ratios.asArray.flat;
        var pmArray = pm.asArray.flat;
        var fmArray = fm.asArray.flat;
        var amplitudeArray = amplitudes.asArray.flat;
        var waveformArray = waveforms.asArray.flat;
        if(ratioArray.size != 3
            or: { pmArray.size != 9 }
            or: { fmArray.size != 9 }
            or: { amplitudeArray.size != 3 }
            or: { waveformArray.size != 3 }) {
            Error(
                "PMNetworkOsc: ratios/amplitudes/waveforms require 3 values and pm/fm require 9."
            ).throw
        };
        waveformArray.do { |waveform|
            if(waveform.isInteger.not
                or: { waveform < 0 }
                or: { waveform > 3 }) {
                Error(
                    "PMNetworkOsc: waveforms must be literal integers from 0 to 3."
                ).throw
            }
        };
        if([1, 2, 4].includes(oversample).not) {
            Error(
                "PMNetworkOsc: oversample must be the literal value 1, 2, or 4."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 3, freq]
            ++ ratioArray ++ pmArray ++ fmArray
            ++ amplitudeArray ++ waveformArray
            ++ [oversample]
        ).collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "PMNetworkOsc is audio-rate only; use PMNetworkOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

ClusterPMOsc : MultiOutUGen {
    *ar {
        |freq = 110, ratios = #[1, 2, 3, 4],
        clusterRatios = #[1, 1.01, 2, 3],
        clusterAmplitudes = #[1, 0.25, 0.15, 0.1],
        pm = #[
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
        ],
        amplitudes = #[1, 1, 1, 1],
        oversample = 2, mul = 1, add = 0|
        var ratioArray = ratios.asArray.flat;
        var clusterRatioArray = clusterRatios.asArray.flat;
        var clusterAmplitudeArray =
            clusterAmplitudes.asArray.flat;
        var pmArray = pm.asArray.flat;
        var amplitudeArray = amplitudes.asArray.flat;
        if(ratioArray.size != 4
            or: { clusterRatioArray.size != 4 }
            or: { clusterAmplitudeArray.size != 4 }
            or: { pmArray.size != 16 }
            or: { amplitudeArray.size != 4 }) {
            Error(
                "ClusterPMOsc: ratios and amplitudes require 4 values; pm requires 16."
            ).throw
        };
        if([1, 2, 4].includes(oversample).not) {
            Error(
                "ClusterPMOsc: oversample must be the literal value 1, 2, or 4."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 4, freq]
            ++ ratioArray ++ clusterRatioArray
            ++ clusterAmplitudeArray ++ pmArray
            ++ amplitudeArray ++ [oversample]
        ).collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "ClusterPMOsc is audio-rate only; use ClusterPMOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
