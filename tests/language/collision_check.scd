(
var names = [
    \HarmonicStrideOsc,
    \TableMorph2D,
    \TZSplitOsc,
    \ScaleSpreadOsc,
    \SpectralBasisOsc,
    \PartialClusterOsc,
    \SyncRingOsc,
    \PMNetworkOsc,
    \ClusterPMOsc,
    \ScannedNetworkOsc,
    \VectorPhaseOsc,
    \SpectralFrameBuffer,
    \SpectralFrameOsc,
    \RippleFormantOsc,
    \ShiftLogicOsc,
    \QuadratureFeedbackOsc,
    \SyncModeOsc,
    \TableMorph3D,
    \RatioFamilyOsc,
    \PitchRegisterOsc
];
var ok = true;

names.do { |name|
    var class = name.asClass;
    var filename;
    if(class.isNil) {
        ("MISSING_CLASS " ++ name).postln;
        ok = false
    } {
        filename = class.filenameSymbol.asString;
        if(filename.contains(
            "NovelOscUGens/Classes/"
        ).not) {
            (
                "CLASS_COLLISION % defined in %"
                .format(name, filename)
            ).postln;
            ok = false
        }
    }
};

if(ok) {
    "NOVEL_OSC_COLLISION_CHECK_OK".postln;
    0.exit
} {
    1.exit
};
)
