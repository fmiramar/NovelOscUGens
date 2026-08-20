(
var pages = [
    "Classes/HarmonicStrideOsc",
    "Classes/TableMorph2D",
    "Classes/TZSplitOsc",
    "Classes/ScaleSpreadOsc",
    "Classes/SpectralBasisOsc",
    "Classes/PartialClusterOsc",
    "Classes/SyncRingOsc",
    "Classes/PMNetworkOsc",
    "Classes/ClusterPMOsc",
    "Classes/ScannedNetworkOsc",
    "Classes/VectorPhaseOsc",
    "Classes/SpectralFrameBuffer",
    "Classes/SpectralFrameOsc",
    "Classes/RippleFormantOsc",
    "Classes/ShiftLogicOsc",
    "Classes/QuadratureFeedbackOsc",
    "Classes/SyncModeOsc",
    "Classes/TableMorph3D",
    "Classes/RatioFamilyOsc",
    "Classes/PitchRegisterOsc",
    "Guides/NovelOscillators"
];
var ok = true;

SCDoc.verbosity = 2;
SCDoc.indexAllDocuments(true);

pages.do { |path|
    var document = SCDoc.documents[path];
    if(document.isNil) {
        ("MISSING_DOC " ++ path).postln;
        ok = false
    } {
        var root = SCDoc.parseDoc(document);
        if(root.isNil) {
            ("PARSE_FAILED " ++ path).postln;
            ok = false
        } {
            SCDoc.renderer.renderToFile(
                document.destPath, document, root
            );
            ("RENDERED " ++ path).postln
        }
    }
};

if(ok) {
    "NOVEL_OSC_SCDOC_OK".postln;
    0.exit
} {
    "NOVEL_OSC_SCDOC_FAILED".postln;
    1.exit
};
)
