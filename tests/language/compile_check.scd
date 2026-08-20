(
var outputs;
var krErrors = 0;
var arrayErrors = 0;
var badTable = 0 ! 64;

SynthDef(\novelOscCompileAll, {
    var mono;
    mono = HarmonicStrideOsc.ar(110);
    if(mono.size != 2) {
        Error("HarmonicStrideOsc output count").throw
    };
    Out.ar(0, mono * 0);

    Out.ar(0, TableMorph2D.ar(
        0, 110, tablesX: 2, tablesY: 2,
        tableSize: 64, mipLevels: 2
    ) * 0);

    outputs = TZSplitOsc.ar(110);
    if(outputs.size != 4) {
        Error("TZSplitOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = ScaleSpreadOsc.ar(
        0, 110, voices: 4
    );
    if(outputs.size != 2) {
        Error("ScaleSpreadOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    Out.ar(0, SpectralBasisOsc.ar(
        110, tableSize: 128
    ) * 0);

    outputs = PartialClusterOsc.ar(
        55, partials: 8, voices: 2, maxPartials: 16
    );
    if(outputs.size != 2) {
        Error("PartialClusterOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = SyncRingOsc.ar(110);
    if(outputs.size != 4) {
        Error("SyncRingOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = PMNetworkOsc.ar(110);
    if(outputs.size != 3) {
        Error("PMNetworkOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = ClusterPMOsc.ar(110);
    if(outputs.size != 4) {
        Error("ClusterPMOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = ScannedNetworkOsc.ar(
        110, nodes: 32, mul: 0.5, add: 0.1
    );
    if(outputs.size != 2) {
        Error("ScannedNetworkOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    Out.ar(0, VectorPhaseOsc.ar(
        110, [0.25, 0.7], [0.4, 0.8]
    ) * 0);
    Out.ar(0, SpectralFrameOsc.ar(
        0, 110, frames: 2, partials: 4
    ) * 0);

    outputs = RippleFormantOsc.ar(110);
    if(outputs.size != 4) {
        Error("RippleFormantOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = ShiftLogicOsc.ar(110, 137);
    if(outputs.size != 5) {
        Error("ShiftLogicOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = QuadratureFeedbackOsc.ar(110);
    if(outputs.size != 4) {
        Error("QuadratureFeedbackOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    Out.ar(0, SyncModeOsc.ar(110, Impulse.ar(7)) * 0);
    Out.ar(0, TableMorph3D.ar(
        0, 110, tablesX: 2, tablesY: 2, tablesZ: 2,
        tableSize: 64, mipLevels: 2
    ) * 0);

    outputs = RatioFamilyOsc.ar(110, [1, 1.5, 2]);
    if(outputs.size != 3) {
        Error("RatioFamilyOsc output count").throw
    };
    Out.ar(0, outputs * 0);

    outputs = PitchRegisterOsc.ar(
        Impulse.ar(2), 0, 0,
        initialDegrees: [0, -2, -4], harmonics: 8
    );
    if(outputs.size != 3) {
        Error("PitchRegisterOsc output count").throw
    };
    Out.ar(0, outputs * 0);
}).asBytes;

[
    HarmonicStrideOsc,
    TableMorph2D,
    TZSplitOsc,
    ScaleSpreadOsc,
    SpectralBasisOsc,
    PartialClusterOsc,
    SyncRingOsc,
    PMNetworkOsc,
    ClusterPMOsc,
    ScannedNetworkOsc,
    VectorPhaseOsc,
    SpectralFrameOsc,
    RippleFormantOsc,
    ShiftLogicOsc,
    QuadratureFeedbackOsc,
    SyncModeOsc,
    TableMorph3D,
    BiroPitchRegister,
    RatioFamilyOsc,
    PitchRegisterOsc
].do { |class|
    try {
        class.kr
    } {
        krErrors = krErrors + 1
    }
};

try {
    PMNetworkOsc.ar(110, ratios: [1, 2])
} {
    arrayErrors = arrayErrors + 1
};
try {
    ClusterPMOsc.ar(110, pm: 0 ! 15)
} {
    arrayErrors = arrayErrors + 1
};
try {
    VectorPhaseOsc.ar(110, [0.5], [0.2, 0.8])
} {
    arrayErrors = arrayErrors + 1
};
try {
    QuadratureFeedbackOsc.ar(110, feedback: [0, 0, 0])
} {
    arrayErrors = arrayErrors + 1
};
try {
    SpectralFrameOsc.ar(0, frames: 65537, partials: 1)
} {
    arrayErrors = arrayErrors + 1
};
try {
    TableMorph3D.ar(
        0, tablesX: 129, tablesY: 1, tablesZ: 1,
        tableSize: 64
    )
} {
    arrayErrors = arrayErrors + 1
};
try {
    TableMorph3D.prepareBuffer(
        Server.default, [], 129, 1, 1, 1
    )
} {
    arrayErrors = arrayErrors + 1
};
try {
    TableMorph2D.prepareBuffer(
        Server.default, [], 1025, 1, 1
    )
} {
    arrayErrors = arrayErrors + 1
};
try {
    SpectralFrameBuffer.prepare(
        Server.default,
        [[[1] ++ (2 ! 1024), [1] ++ (0 ! 1024)]]
    )
} {
    arrayErrors = arrayErrors + 1
};
badTable[7] = inf;
try {
    TableMorph3D.prepareBuffer(
        Server.default, [badTable], 1, 1, 1, 1
    )
} {
    arrayErrors = arrayErrors + 1
};

if(krErrors != 20) {
    Error("not every .kr method rejected construction").throw
};
if(arrayErrors != 10) {
    Error("fixed-array validation did not reject errors").throw
};

"NOVEL_OSC_LANGUAGE_COMPILE_OK".postln;
0.exit;
)
