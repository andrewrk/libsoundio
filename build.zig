const std = @import("std");

const Sdk = @import("Sdk.zig");

pub fn build(b: *std.build.Builder) void {
    const mode = b.standardReleaseOptions();
    const target = b.standardTargetOptions(.{});
    const sdk = Sdk.init(b);

    const build_static_libs = b.option(bool, "static-libs", "Build static libraries") orelse true;
    const build_dynamic_libs = b.option(bool, "dynamic-libs", "Build dynamic libraries") orelse true;
    const build_example_programs = b.option(bool, "examples", "Build example programs") orelse true;
    const build_tests = b.option(bool, "tests", "Build tests") orelse true;
    const enable_jack = b.option(bool, "jack", "Enable JACK backend") orelse true;
    const enable_pulseaudio = b.option(bool, "pulseaudio", "Enable PulseAudio backend") orelse true;
    const enable_alsa = b.option(bool, "alsa", "Enable ALSA backend") orelse true;
    const enable_coreaudio = b.option(bool, "coreaudio", "Enable CoreAudio backend") orelse true;
    const enable_wasapi = b.option(bool, "wasapi", "Enable WASAPI backend") orelse true;

    sdk.installHeaders();

    const lib_config = Sdk.LibConfig{
        .jack = enable_jack,
        .pulseaudio = enable_pulseaudio,
        .alsa = enable_alsa,
        .coreaudio = enable_coreaudio,
        .wasapi = enable_wasapi,
    };

    const static_lib = sdk.createLibrary(target, .Static, lib_config);
    static_lib.setBuildMode(mode);
    if (build_static_libs)
        static_lib.install();

    const dynamic_lib = sdk.createLibrary(target, .Dynamic, lib_config);
    dynamic_lib.setBuildMode(mode);
    if (build_dynamic_libs)
        dynamic_lib.install();

    if (build_example_programs) {
        const Example = struct { name: []const u8, file: []const u8 };
        const examples = [_]Example{
            .{ .name = "sio_sine", .file = "example/sio_sine.c" },
            .{ .name = "sio_list_devices", .file = "example/sio_list_devices.c" },
            .{ .name = "sio_microphone", .file = "example/sio_microphone.c" },
            .{ .name = "sio_record", .file = "example/sio_record.c" },
        };

        for (examples) |example| {
            const exe = b.addExecutable(example.name, example.file);
            for (sdk.getIncludePaths()) |path| {
                exe.addIncludePath(path);
            }
            exe.linkLibC();

            if (build_dynamic_libs) {
                exe.linkLibrary(dynamic_lib);
            } else {
                exe.linkLibrary(static_lib);
            }

            exe.install();
        }
    }

    if (build_tests) {
        const test_step = b.step("test", "Runs the test suite.");

        const Test = struct { name: []const u8, file: []const u8 };
        const tests = [_]Test{
            .{ .name = "underflow", .file = "test/underflow.c" },
            .{ .name = "backend_disconnect_recover", .file = "test/backend_disconnect_recover.c" },
            .{ .name = "overflow", .file = "test/overflow.c" },
        };

        for (tests) |tst| {
            const exe = b.addExecutable(tst.name, tst.file);
            for (sdk.getIncludePaths()) |path| {
                exe.addIncludePath(path);
            }
            exe.linkLibC();

            if (build_dynamic_libs) {
                exe.linkLibrary(dynamic_lib);
            } else {
                exe.linkLibrary(static_lib);
            }

            exe.install();

            const run_test = exe.run();
            test_step.dependOn(&run_test.step);
        }

        // TODO: Implement unit tests and code coverage
        // unit_tests "${libsoundio_SOURCE_DIR}/test/unit_tests.c" ${LIBSOUNDIO_SOURCES})
        // latency "${libsoundio_SOURCE_DIR}/test/latency.c" ${LIBSOUNDIO_SOURCES})

        // const coverage_step = b.step("coverage", "Performs a code coverage test (Requires lcov)");
    }

    // TODO: Implement doxygen generation
    // const doc_step = b.step("docs", "Builds the documentation (Requires doxygen)");
    //
    // const make_docs = b.addSystemCommand(&.{"doxygen"});
}
