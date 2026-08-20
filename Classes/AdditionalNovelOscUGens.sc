ScannedNetworkOsc : MultiOutUGen {
    *ar {
        |freq = 110, rate = 0.2, stiffness = 0.25,
        centering = 0.01, damping = 0.02, excite = 0,
        strike = 0, strikeAmp = 1, position = 0.5,
        scanPath = 0, nodes = 128, topology = 1,
        initShape = 1, seed = 0, mul = 1, add = 0|
        var result;
        [scanPath, nodes, topology, initShape, seed].do {
            |value|
            if(value.isInteger.not) {
                Error(
                    "ScannedNetworkOsc: scanPath, nodes, topology, initShape, and seed must be literal integers."
                ).throw
            }
        };
        if(nodes < 16 or: { nodes > 512 }) {
            Error(
                "ScannedNetworkOsc: nodes must be from 16 to 512."
            ).throw
        };
        if((0..2).includes(scanPath).not
            or: { (0..1).includes(topology).not }
            or: { (0..3).includes(initShape).not }
            or: { seed < 0 }) {
            Error(
                "ScannedNetworkOsc: invalid initialization mode."
            ).throw
        };
        result = this.multiNewList(
            [\audio, 2, freq, rate, stiffness, centering,
                damping, excite, strike, strikeAmp, position,
                scanPath, nodes, topology, initShape, seed]
        );
        ^result.collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "ScannedNetworkOsc is audio-rate only; use ScannedNetworkOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

VectorPhaseOsc : UGen {
    *ar {
        |freq = 440, x = #[0.5], y = #[0.5],
        phaseMod = 0, sync = 0, oversample = 2,
        mul = 1, add = 0|
        var xValues = x.asArray.flat;
        var yValues = y.asArray.flat;
        if(xValues.size != yValues.size
            or: { xValues.size < 1 }
            or: { xValues.size > 8 }) {
            Error(
                "VectorPhaseOsc: x and y must have equal literal lengths from 1 to 8."
            ).throw
        };
        if([1, 2, 4].includes(oversample).not) {
            Error(
                "VectorPhaseOsc: oversample must be the literal value 1, 2, or 4."
            ).throw
        };
        ^this.multiNewList(
            [\audio, freq, phaseMod, sync, oversample,
                xValues.size] ++ xValues ++ yValues
        ).madd(mul, add)
    }

    *kr {
        Error(
            "VectorPhaseOsc is audio-rate only; use VectorPhaseOsc.ar."
        ).throw
    }

    checkInputs { ^this.checkValidInputs }
}

SpectralFrameBuffer : Object {
    var <buffer, <frames, <partials;

    *new { |buffer, frames, partials|
        ^super.new.init(buffer, frames, partials)
    }

    *prepare {
        |server, frameData, normalize = true, action|
        var sourceFrames, partialCount, flattened, object;
        var allocatedBuffer;
        server = server ? Server.default;
        sourceFrames = frameData.asArray;
        if(sourceFrames.isEmpty
            or: { sourceFrames.size > 65536 }) {
            Error(
                "SpectralFrameBuffer.prepare: provide from 1 to 65536 frames."
            ).throw
        };
        sourceFrames = sourceFrames.collect { |frame, frameIndex|
            var pair = frame.asArray;
            var ratios, amplitudes, energy;
            if(pair.size != 2) {
                Error(
                    "SpectralFrameBuffer.prepare: frame % must contain [ratios, amplitudes]."
                    .format(frameIndex)
                ).throw
            };
            ratios = pair[0].asArray.flat;
            amplitudes = pair[1].asArray.flat;
            if(ratios.isEmpty or: { ratios.size != amplitudes.size }) {
                Error(
                    "SpectralFrameBuffer.prepare: every frame needs equally sized, nonempty ratio and amplitude arrays."
                ).throw
            };
            ratios.do { |value|
                if(value.isNumber.not
                    or: { value <= 0 }
                    or: { value.isNaN }
                    or: { value.abs >= inf }) {
                    Error(
                        "SpectralFrameBuffer.prepare: ratios must be positive and finite."
                    ).throw
                }
            };
            amplitudes.do { |value|
                if(value.isNumber.not
                    or: { value.isNaN }
                    or: { value.abs >= inf }) {
                    Error(
                        "SpectralFrameBuffer.prepare: amplitudes must be finite."
                    ).throw
                }
            };
            energy = amplitudes.sum { |value| value.squared }.sqrt;
            if(normalize and: { energy > 0 }) {
                amplitudes = amplitudes / energy
            };
            [ratios, amplitudes]
        };
        partialCount = sourceFrames[0][0].size;
        if(partialCount > 1024) {
            Error(
                "SpectralFrameBuffer.prepare: frames may contain at most 1024 partials."
            ).throw
        };
        if(sourceFrames.any { |frame|
            frame[0].size != partialCount
        }) {
            Error(
                "SpectralFrameBuffer.prepare: all frames must have the same partial count."
            ).throw
        };
        flattened = sourceFrames.collect { |frame|
            frame.flop.flat
        }.flat.as(FloatArray);
        allocatedBuffer = Buffer.loadCollection(
            server, flattened, 1,
            action: { |loadedBuffer|
                object = this.new(
                    loadedBuffer, sourceFrames.size, partialCount
                );
                action.value(object)
            }
        );
        ^allocatedBuffer
    }

    init { |aBuffer, frameCount, partialCount|
        buffer = aBuffer;
        frames = frameCount;
        partials = partialCount;
        ^this
    }

    free {
        buffer.free;
        buffer = nil
    }
}

SpectralFrameOsc : UGen {
    *ar {
        |bufnum, freq = 110, frame = 0, stretch = 1,
        tilt = 0, blur = 0, activePartials = 64,
        phaseMode = 0, frames = 1, partials = 64,
        seed = 0, mul = 1, add = 0|
        [phaseMode, frames, partials, seed].do { |value|
            if(value.isInteger.not) {
                Error(
                    "SpectralFrameOsc: phaseMode, frames, partials, and seed must be literal integers."
                ).throw
            }
        };
        if((0..2).includes(phaseMode).not
            or: { frames < 1 }
            or: { frames > 65536 }
            or: { partials < 1 }
            or: { partials > 1024 }
            or: { seed < 0 }) {
            Error(
                "SpectralFrameOsc: invalid frame-bank initialization argument."
            ).throw
        };
        ^this.multiNewList(
            [\audio, bufnum, freq, frame, stretch, tilt,
                blur, activePartials, phaseMode, frames,
                partials, seed]
        ).madd(mul, add)
    }

    *kr {
        Error(
            "SpectralFrameOsc is audio-rate only; use SpectralFrameOsc.ar."
        ).throw
    }

    checkInputs { ^this.checkValidInputs }
}

RippleFormantOsc : MultiOutUGen {
    *ar {
        |freq = 110, formantA = 700, formantB = 1200,
        decayA = 0.01, decayB = 0.008,
        sweepA = 0, sweepB = 0, balance = 0.5,
        baseLevel = 0.2, rippleLevel = 1, phase = 0,
        oversample = 2, mul = 1, add = 0|
        var result;
        if([1, 2, 4].includes(oversample).not) {
            Error(
                "RippleFormantOsc: oversample must be the literal value 1, 2, or 4."
            ).throw
        };
        result = this.multiNewList(
            [\audio, 4, freq, formantA, formantB,
                decayA, decayB, sweepA, sweepB, balance,
                baseLevel, rippleLevel, phase, oversample]
        );
        ^result.collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "RippleFormantOsc is audio-rate only; use RippleFormantOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

ShiftLogicOsc : MultiOutUGen {
    *ar {
        |freqA = 110, freqB = 137,
        feedbackA = 0.5, feedbackB = 0.5,
        compare = 0, change = 1, slew = 0.01,
        bits = 8, dacBits = 3, mode = 1, seed = 0,
        mul = 1, add = 0|
        var result;
        [bits, dacBits, mode, seed].do { |value|
            if(value.isInteger.not) {
                Error(
                    "ShiftLogicOsc: bits, dacBits, mode, and seed must be literal integers."
                ).throw
            }
        };
        if(bits < 3 or: { bits > 32 }
            or: { dacBits < 1 }
            or: { dacBits > bits }
            or: { (0..3).includes(mode).not }
            or: { seed < 0 }) {
            Error(
                "ShiftLogicOsc: invalid register initialization argument."
            ).throw
        };
        result = this.multiNewList(
            [\audio, 5, freqA, freqB, feedbackA, feedbackB,
                compare, change, slew, bits, dacBits, mode, seed]
        );
        ^result.collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "ShiftLogicOsc is audio-rate only; use ShiftLogicOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

QuadratureFeedbackOsc : MultiOutUGen {
    *ar {
        |freq = 110, input = 0,
        feedback = #[0, 0, 0, 0], inject = 0,
        stability = 1, drive = 1, oversample = 4,
        mul = 1, add = 0|
        var values = feedback.asArray.flat;
        var result;
        if(values.size != 4) {
            Error(
                "QuadratureFeedbackOsc: feedback must contain exactly four values."
            ).throw
        };
        if([1, 2, 4, 8].includes(oversample).not) {
            Error(
                "QuadratureFeedbackOsc: oversample must be the literal value 1, 2, 4, or 8."
            ).throw
        };
        result = this.multiNewList(
            [\audio, 4, freq, input] ++ values
                ++ [inject, stability, drive, oversample]
        );
        ^result.collect { |channel| channel.madd(mul, add) }
    }

    *kr {
        Error(
            "QuadratureFeedbackOsc is audio-rate only; use QuadratureFeedbackOsc.ar."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

SyncModeOsc : UGen {
    *ar {
        |freq = 110, sync = 0, mode = 0,
        resetPhase = 0, symmetry = 0.5, shape = 0,
        softness = 0.02, oversample = 2,
        mul = 1, add = 0|
        if(mode.isInteger.not
            or: { (0..5).includes(mode).not }
            or: { shape.isInteger.not }
            or: { (0..3).includes(shape).not }) {
            Error(
                "SyncModeOsc: mode and shape must be literal integers in their documented ranges."
            ).throw
        };
        if([1, 2, 4].includes(oversample).not) {
            Error(
                "SyncModeOsc: oversample must be the literal value 1, 2, or 4."
            ).throw
        };
        ^this.multiNewList(
            [\audio, freq, sync, mode, resetPhase,
                symmetry, shape, softness, oversample]
        ).madd(mul, add)
    }

    *kr {
        Error(
            "SyncModeOsc is audio-rate only; use SyncModeOsc.ar."
        ).throw
    }

    checkInputs { ^this.checkValidInputs }
}

TableMorph3D : UGen {
    *ar {
        |bufnum, freq = 440, x = 0, y = 0, z = 0,
        phase = 0, tablesX = 4, tablesY = 4,
        tablesZ = 4, tableSize = 2048, mipLevels = 1,
        interpolation = 2, mul = 1, add = 0|
        [tablesX, tablesY, tablesZ, tableSize,
            mipLevels, interpolation].do { |value|
            if(value.isInteger.not) {
                Error(
                    "TableMorph3D: layout and interpolation arguments must be literal integers."
                ).throw
            }
        };
        if(tablesX < 1 or: { tablesX > 128 }
            or: { tablesY < 1 }
            or: { tablesY > 128 }
            or: { tablesZ < 1 }
            or: { tablesZ > 128 }) {
            Error(
                "TableMorph3D: table dimensions must be from 1 to 128."
            ).throw
        };
        if(tableSize < 64
            or: { tableSize > 65536 }
            or: { tableSize.isPowerOfTwo.not }) {
            Error(
                "TableMorph3D: tableSize must be a power of two from 64 to 65536."
            ).throw
        };
        if(mipLevels < 1 or: { mipLevels > 16 }
            or: { (0..2).includes(interpolation).not }) {
            Error(
                "TableMorph3D: invalid mipLevels or interpolation."
            ).throw
        };
        ^this.multiNewList(
            [\audio, bufnum, freq, x, y, z, phase,
                tablesX, tablesY, tablesZ, tableSize,
                mipLevels, interpolation]
        ).madd(mul, add)
    }

    *kr {
        Error(
            "TableMorph3D is audio-rate only; use TableMorph3D.ar."
        ).throw
    }

    *prepareBuffer {
        |server, tables, tablesX, tablesY, tablesZ,
        mipLevels = 8, normalize = true, action|
        var sourceTables, tableSize, expectedTables;
        var expectedSamples;
        var processed, cosineTable, flattened, position;
        server = server ? Server.default;
        [tablesX, tablesY, tablesZ, mipLevels].do { |value|
            if(value.isInteger.not) {
                Error(
                    "TableMorph3D.prepareBuffer: dimensions and mipLevels must be literal integers."
                ).throw
            }
        };
        if(tablesX < 1 or: { tablesX > 128 }
            or: { tablesY < 1 }
            or: { tablesY > 128 }
            or: { tablesZ < 1 }
            or: { tablesZ > 128 }
            or: { mipLevels < 1 }
            or: { mipLevels > 16 }) {
            Error(
                "TableMorph3D.prepareBuffer: dimensions must be from 1 to 128 and mipLevels from 1 to 16."
            ).throw
        };
        sourceTables = tables.asArray.collect {
            |table| table.asArray
        };
        expectedTables = tablesX * tablesY * tablesZ;
        if(sourceTables.size != expectedTables
            or: { sourceTables.isEmpty }
            or: { sourceTables[0].isEmpty }) {
            Error(
                "TableMorph3D.prepareBuffer: table count or content is invalid."
            ).throw
        };
        tableSize = sourceTables[0].size;
        if(tableSize < 64
            or: { tableSize > 65536 }
            or: { tableSize.isPowerOfTwo.not }
            or: { sourceTables.any { |table|
                table.size != tableSize
            } }) {
            Error(
                "TableMorph3D.prepareBuffer: all tables must share a power-of-two size from 64 to 65536."
            ).throw
        };
        expectedSamples =
            expectedTables * tableSize * mipLevels;
        if(expectedSamples > 2147483647) {
            Error(
                "TableMorph3D.prepareBuffer: the declared layout exceeds the server Buffer sample limit."
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
                "TableMorph3D.prepareBuffer: every table sample must be finite."
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
                    TableMorph2D.makeMip(
                        table, level, cosineTable
                    )
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

    checkInputs { ^this.checkValidInputs }
}

RatioFamilyOsc : Object {
    *ar {
        |freq = 110,
        ratios = #[1, 1.2, 1.5, 2, 3, 4],
        spread = 1, skew = 0.5, curve = 0,
        linearFM = 0, phase = 0, mul = 1, add = 0|
        var values = ratios.asArray.flat;
        var safeSkew = skew.clip(0.001, 0.999);
        var safeCurve = curve.clip(-1, 1);
        if(values.size < 1 or: { values.size > 32 }) {
            Error(
                "RatioFamilyOsc: ratios must contain from 1 to 32 values."
            ).throw
        };
        values.do { |value|
            if(value.isNumber.not
                or: { value <= 0 }
                or: { value.isNaN }
                or: { value.abs >= inf }) {
                Error(
                    "RatioFamilyOsc: ratios must be positive finite literals."
                ).throw
            }
        };
        ^values.collect { |ratio|
            var spreadRatio = 2 ** (ratio.log2 * spread);
            var frequency = (freq * spreadRatio) + linearFM;
            var accumulated = Phasor.ar(
                0, frequency / SampleRate.ir,
                0, 1, phase * (2pi).reciprocal
            );
            var leftPosition = accumulated / safeSkew;
            var rightPosition =
                (accumulated - safeSkew) / (1 - safeSkew);
            var left = 0.5 * (
                leftPosition
                + (safeCurve * leftPosition
                    * (1 - leftPosition))
            );
            var right = 0.5 + (0.5 * (
                rightPosition
                - (safeCurve * rightPosition
                    * (1 - rightPosition))
            ));
            var mapped = Select.ar(
                accumulated >= safeSkew, [left, right]
            );
            SinOsc.ar(0, mapped * 2pi).madd(mul, add)
        }
    }

    *kr {
        Error(
            "RatioFamilyOsc is audio-rate only; use RatioFamilyOsc.ar."
        ).throw
    }
}

BiroPitchRegister : MultiOutUGen {
    *ar { |trig, degree, initialDegrees|
        var values = initialDegrees.asArray.flat;
        ^this.multiNewList(
            [\audio, values.size, trig, degree, values.size]
                ++ values
        )
    }

    *kr {
        Error(
            "BiroPitchRegister is audio-rate only."
        ).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

PitchRegisterOsc : Object {
    *ar {
        |trig, degree, scaleBuf, root = 110,
        initialDegrees = #[0, -2, -4],
        harmonics = 16, decay = 0.8, glide = 0.02,
        mul = 1, add = 0|
        var values = initialDegrees.asArray.flat;
        var stored, semitones, frequencies;
        var preferred = \HarmonicStrideOsc.asClass;
        if(values.size < 1 or: { values.size > 32 }) {
            Error(
                "PitchRegisterOsc: initialDegrees must contain from 1 to 32 values."
            ).throw
        };
        values.do { |value|
            if(value.isNumber.not
                or: { value.isNaN }
                or: { value.abs >= inf }) {
                Error(
                    "PitchRegisterOsc: initialDegrees must be finite literals."
                ).throw
            }
        };
        if(harmonics.isInteger.not
            or: { harmonics < 1 }
            or: { harmonics > 16384 }) {
            Error(
                "PitchRegisterOsc: harmonics must be a literal integer from 1 to 16384."
            ).throw
        };
        stored = BiroPitchRegister.ar(trig, degree, values);
        semitones = DegreeToKey.ar(scaleBuf, stored, 12);
        frequencies = Lag.ar(
            root * (2 ** (semitones / 12)),
            glide.max(0)
        );
        ^frequencies.collect { |frequency, index|
            var phaseOffset =
                index / values.size;
            var signal = if(preferred.notNil) {
                preferred.ar(
                    frequency, 1, decay,
                    phaseOffset * 2pi, 1, harmonics
                )[0]
            } {
                var raw = Blip.ar(frequency, harmonics);
                DelayC.ar(
                    raw, 0.1,
                    (phaseOffset / frequency.abs.max(10))
                        .clip(0, 0.1)
                )
            };
            signal.madd(mul, add)
        }
    }

    *kr {
        Error(
            "PitchRegisterOsc is audio-rate only; use PitchRegisterOsc.ar."
        ).throw
    }
}
