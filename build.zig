const std = @import("std");

pub fn build(b: *std.build.Builder) void {
    const target = b.standardTargetOptions(.{});
    const mode = b.standardReleaseOptions();

    const pulseaudio_dep = b.dependency("pulseaudio", .{});

    const lib = b.addStaticLibrary("soundio", null);
    lib.setTarget(target);
    lib.setBuildMode(mode);
    lib.linkLibC();
    lib.linkLibrary(pulseaudio_dep.artifact("pulse"));
    lib.addIncludePath(".");
    lib.addConfigHeader(b.addConfigHeader(.{ .path = "src/config.h.in" }, .cmake, .{
        .SOUNDIO_HAVE_JACK = null,
        .SOUNDIO_HAVE_PULSEAUDIO = {},
        .SOUNDIO_HAVE_ALSA = null,
        .SOUNDIO_HAVE_COREAUDIO = null,
        .SOUNDIO_HAVE_WASAPI = null,

        .SOUNDIO_VERSION_MAJOR = 2,
        .SOUNDIO_VERSION_MINOR = 0,
        .SOUNDIO_VERSION_PATCH = 0,
        .SOUNDIO_VERSION_STRING = "2.0.0",
    }));
    lib.addCSourceFiles(&.{
        "src/soundio.c",
        "src/util.c",
        "src/os.c",
        "src/dummy.c",
        "src/channel_layout.c",
        "src/ring_buffer.c",
        "src/pulseaudio.c",
    }, &.{
        "-std=c11",
        "-fvisibility=hidden",
        "-Wall",
        "-Werror=strict-prototypes",
        "-Werror=old-style-definition",
        "-Werror=missing-prototypes",
        "-D_REENTRANT",
        "-D_POSIX_C_SOURCE=200809L",
        "-Wno-missing-braces",
    });
    lib.install();
    lib.installHeadersDirectory("soundio", "soundio");
}
