const std = @import("std");

pub fn build(b: *std.build.Builder) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const pulseaudio_dep = b.dependency("pulseaudio", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "soundio",
        .target = target,
        .optimize = optimize,
    });
    lib.linkLibC();
    lib.linkLibrary(pulseaudio_dep.artifact("pulse"));
    lib.addIncludePath(".");
    lib.addConfigHeader(b.addConfigHeader(.{
        .style = .{ .cmake = .{ .path = "src/config.h.in" } },
    }, .{
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

    const sio_list_devices = b.addExecutable(.{
        .name = "sio_list_devices",
        .target = target,
        .optimize = optimize,
    });
    sio_list_devices.addCSourceFiles(&.{"example/sio_list_devices.c"}, &.{});
    sio_list_devices.linkLibrary(lib);
    sio_list_devices.install();

    const sio_microphone = b.addExecutable(.{
        .name = "sio_microphone",
        .target = target,
        .optimize = optimize,
    });
    sio_microphone.addCSourceFiles(&.{"example/sio_microphone.c"}, &.{});
    sio_microphone.linkLibrary(lib);
    sio_microphone.install();

    const sio_record = b.addExecutable(.{
        .name = "sio_record",
        .target = target,
        .optimize = optimize,
    });
    sio_record.addCSourceFiles(&.{"example/sio_record.c"}, &.{});
    sio_record.linkLibrary(lib);
    sio_record.install();

    const sio_sine = b.addExecutable(.{
        .name = "sio_sine",
        .target = target,
        .optimize = optimize,
    });
    sio_sine.addCSourceFiles(&.{"example/sio_sine.c"}, &.{});
    sio_sine.linkLibrary(lib);
    sio_sine.install();
}
