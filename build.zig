const std = @import("std");

const flags: []const []const u8 = &.{
    "-std=c11",
    "-fvisibility=hidden",
    "-Wall",
    "-Werror=strict-prototypes",
    "-Werror=old-style-definition",
    "-Werror=missing-prototypes",
    "-D_REENTRANT",
    "-D_POSIX_C_SOURCE=200809L",
    "-Wno-missing-braces",
};

pub fn build(b: *std.build.Builder) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "soundio",
        .target = target,
        .optimize = optimize,
    });

    const os_tag = target.getOsTag();

    switch (os_tag) {
        .linux => {
            const pulseaudio_dep = b.dependency("pulseaudio", .{
                .target = target,
                .optimize = optimize,
            });

            lib.linkLibrary(pulseaudio_dep.artifact("pulse"));
            lib.addCSourceFile(.{
                .file = .{ .path = "src/pulseaudio.c" },
                .flags = flags,
            });
        },
        .macos => {
            lib.linkFramework("CoreFoundation");
            lib.linkFramework("CoreAudio");
            lib.linkFramework("AudioUnit");
            lib.addCSourceFile(.{
                .file = .{ .path = "src/coreaudio.c" },
                .flags = flags,
            });
        },
        .windows => {
            lib.addCSourceFile(.{
                .file = .{ .path = "src/wasapi.c" },
                .flags = flags,
            });

            lib.linkSystemLibrary("ole32");
            b.installArtifact(lib);
        },
        else => @panic("unsupported OS"),
    }

    lib.linkLibC();
    lib.addIncludePath(.{ .path = "." });
    lib.addConfigHeader(b.addConfigHeader(.{
        .style = .{ .cmake = .{ .path = "src/config.h.in" } },
    }, .{
        .SOUNDIO_HAVE_JACK = null,
        .SOUNDIO_HAVE_PULSEAUDIO = if (os_tag == .linux) {} else null,
        .SOUNDIO_HAVE_ALSA = null,
        .SOUNDIO_HAVE_COREAUDIO = if (os_tag == .macos) {} else null,
        .SOUNDIO_HAVE_WASAPI = if (os_tag == .windows) {} else null,

        .LIBSOUNDIO_VERSION_MAJOR = 2,
        .LIBSOUNDIO_VERSION_MINOR = 0,
        .LIBSOUNDIO_VERSION_PATCH = 0,
        .LIBSOUNDIO_VERSION = "2.0.0",
    }));
    lib.addCSourceFiles(.{
        .files = &.{
            "src/soundio.c",
            "src/util.c",
            "src/os.c",
            "src/dummy.c",
            "src/channel_layout.c",
            "src/ring_buffer.c",
        },
        .flags = flags,
    });
    //lib.defineCMacro("SOUNDIO_STATIC_LIBRARY", null);
    b.installArtifact(lib);
    lib.installHeadersDirectory("soundio", "soundio");

    const sio_list_devices = b.addExecutable(.{
        .name = "sio_list_devices",
        .target = target,
        .optimize = optimize,
    });
    sio_list_devices.addCSourceFiles(.{
        .files = &.{"example/sio_list_devices.c"},
    });
    sio_list_devices.defineCMacro("SOUNDIO_STATIC_LIBRARY", null);
    sio_list_devices.linkLibrary(lib);
    b.installArtifact(sio_list_devices);

    const sio_microphone = b.addExecutable(.{
        .name = "sio_microphone",
        .target = target,
        .optimize = optimize,
    });
    sio_microphone.addCSourceFiles(.{
        .files = &.{"example/sio_microphone.c"},
    });
    sio_microphone.defineCMacro("SOUNDIO_STATIC_LIBRARY", null);
    sio_microphone.linkLibrary(lib);
    b.installArtifact(sio_microphone);

    const sio_record = b.addExecutable(.{
        .name = "sio_record",
        .target = target,
        .optimize = optimize,
    });
    sio_record.addCSourceFiles(.{
        .files = &.{"example/sio_record.c"},
    });
    sio_record.defineCMacro("SOUNDIO_STATIC_LIBRARY", null);
    sio_record.linkLibrary(lib);
    b.installArtifact(sio_record);

    const sio_sine = b.addExecutable(.{
        .name = "sio_sine",
        .target = target,
        .optimize = optimize,
    });
    sio_sine.addCSourceFiles(.{
        .files = &.{"example/sio_sine.c"},
    });
    sio_sine.defineCMacro("SOUNDIO_STATIC_LIBRARY", null);
    sio_sine.linkLibrary(lib);
    b.installArtifact(sio_sine);
}
