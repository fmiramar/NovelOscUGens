(
var root = thisProcess.nowExecutingPath.dirname.dirname;
var paths = [
    root +/+ "language/compile_check.scd",
    root +/+ "language/collision_check.scd",
    root +/+ "language/additional_live_smoke.scd",
    root +/+ "language/additional_supernova_smoke.scd",
    root +/+ "language/live_smoke.scd",
    root +/+ "language/randomized_safety.scd",
    root +/+ "language/supernova_smoke.scd",
    root +/+ "language/verify_scdoc.scd",
    root +/+ "language/compile_help_examples.scd",
    root +/+ "nrt/render_metrics.scd",
    root +/+ "benchmarks/realtime_cpu.scd",
    root +/+ "benchmarks/additional_realtime_cpu.scd"
];
var ok = true;

paths.do { |path|
    if(File.readAllString(path).compile.isNil) {
        ("TEST_SCRIPT_COMPILE_FAILED " ++ path).postln;
        ok = false
    }
};

if(ok) {
    "NOVEL_OSC_TEST_SCRIPTS_COMPILE_OK".postln;
    0.exit
} {
    1.exit
};
)
