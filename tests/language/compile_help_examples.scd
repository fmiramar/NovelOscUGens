(
var sourceRoot = ScannedNetworkOsc.filenameSymbol.asString
    .dirname.dirname +/+ "HelpSource";
var files = (
    PathName(sourceRoot +/+ "Classes").files
    ++ PathName(sourceRoot +/+ "Guides").files
).select { |path| path.extension == "schelp" };
var compiled = 0;
var ok = true;

files.do { |path|
    var text = File.readAllString(path.fullPath);
    var offset = 0;
    var marker = "code::\n(";
    var start = text.find(marker, false, offset);

    while { start.notNil } {
        var finish = text.find(
            "\n::", false, start + marker.size
        );
        if(finish.isNil) {
            ("UNTERMINATED_CODE " ++ path.fileName).postln;
            ok = false;
            start = nil
        } {
            var snippet = text.copyRange(
                start + "code::\n".size,
                finish - 1
            );
            try {
                snippet.compile;
                compiled = compiled + 1
            } { |error|
                (
                    "EXAMPLE_COMPILE_FAILED "
                    ++ path.fileName
                ).postln;
                error.reportError;
                ok = false
            };
            offset = finish + 3;
            start = text.find(marker, false, offset)
        }
    }
};

if(compiled != 41) {
    (
        "Expected 41 runnable help examples, compiled "
        ++ compiled
    ).postln;
    ok = false
};

if(ok) {
    "NOVEL_OSC_HELP_EXAMPLES_OK".postln;
    0.exit
} {
    "NOVEL_OSC_HELP_EXAMPLES_FAILED".postln;
    1.exit
};
)
