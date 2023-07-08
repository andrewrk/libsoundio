const std = @import("std");

const Sdk = @This();

pub const library_version = std.builtin.Version{
    .major = 2,
    .minor = 0,
    .patch = 0,
};

const c_flags = [_][]const u8{
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

fn sdkPath(comptime suffix: []const u8) []const u8 {
    if (suffix[0] != '/') @compileError("sdkPath requires an absolute path!");
    return comptime blk: {
        const root_dir = std.fs.path.dirname(@src().file) orelse ".";
        break :blk root_dir ++ suffix;
    };
}

builder: *std.build.Builder,

pub fn init(b: *std.build.Builder) *Sdk {
    const sdk = b.allocator.create(Sdk) catch @panic("out of memory");
    sdk.* = Sdk{
        .builder = b,
    };
    return sdk;
}

pub fn installHeaders(sdk: *Sdk) void {
    for (headers) |hdr| {
        const copy_header = sdk.builder.addInstallFile(
            .{ .path = hdr },
            sdk.builder.fmt("include/soundio/{s}", .{std.fs.path.basename(hdr)}),
        );
        sdk.builder.getInstallStep().dependOn(&copy_header.step);
    }
}

const include_paths = [_][]const u8{
    sdkPath("/"),
};

pub fn getIncludePaths(sdk: *Sdk) []const []const u8 {
    _ = sdk;
    return &include_paths;
}

pub const LibConfig = struct {
    jack: bool = true,
    pulseaudio: bool = true,
    alsa: bool = true,
    coreaudio: bool = true,
    wasapi: bool = true,
};

pub fn createLibrary(sdk: *Sdk, target: std.zig.CrossTarget, linkage: std.builtin.LinkMode, config: LibConfig) *std.build.LibExeObjStep {
    const os_config = LibConfig{
        .jack = config.jack and target.isLinux(),
        .pulseaudio = config.pulseaudio and target.isLinux(),
        .alsa = config.alsa and target.isLinux(),
        .coreaudio = config.coreaudio and target.isDarwin(),
        .wasapi = config.wasapi and target.isWindows(),
    };

    // compute a target path that is definitly unique per build config, so we can
    // safely put and overwrite the config.h file in it.
    const target_tag = sdk.builder.fmt("{s}/{s}/soundio-include-{}.{}.{}-{s}{s}{s}{s}{s}", .{
        sdk.builder.build_root,
        sdk.builder.cache_root,
        library_version.major,
        library_version.minor,
        library_version.patch,
        if (os_config.jack) "j" else "",
        if (os_config.pulseaudio) "p" else "",
        if (os_config.alsa) "a" else "",
        if (os_config.coreaudio) "c" else "",
        if (os_config.wasapi) "w" else "",
    });

    {
        var dir = std.fs.cwd().makeOpenPath(target_tag, .{}) catch @panic("i/o error");
        defer dir.close();

        var file = dir.createFile("config.h", .{}) catch @panic("i/o error");
        defer file.close();

        var writer = file.writer();

        writer.print(
            \\/*
            \\ * Copyright (c) 2015 Andrew Kelley
            \\ *
            \\ * This file is part of libsoundio, which is MIT licensed.
            \\ * See http://opensource.org/licenses/MIT
            \\ */
            \\
            \\#ifndef SOUNDIO_CONFIG_H
            \\#define SOUNDIO_CONFIG_H
            \\
            \\#define SOUNDIO_VERSION_MAJOR {[major]d}
            \\#define SOUNDIO_VERSION_MINOR {[minor]d}
            \\#define SOUNDIO_VERSION_PATCH {[patch]d}
            \\#define SOUNDIO_VERSION_STRING "{[major]d}.{[minor]d}.{[patch]d}"
            \\
            \\{[jack]s}{[pulseaudio]s}{[alsa]s}{[coreaudio]s}{[wasapi]s}
            \\
            \\#endif
        , .{
            .major = library_version.major,
            .minor = library_version.minor,
            .patch = library_version.patch,
            .jack = if (os_config.jack) "#define SOUNDIO_HAVE_JACK\n" else "",
            .pulseaudio = if (os_config.pulseaudio) "#define SOUNDIO_HAVE_PULSEAUDIO\n" else "",
            .alsa = if (os_config.alsa) "#define SOUNDIO_HAVE_ALSA\n" else "",
            .coreaudio = if (os_config.coreaudio) "#define SOUNDIO_HAVE_COREAUDIO\n" else "",
            .wasapi = if (os_config.wasapi) "#define SOUNDIO_HAVE_WASAPI\n" else "",
        }) catch @panic("i/o error");
    }

    const lib: *std.build.LibExeObjStep = switch (linkage) {
        .Dynamic => sdk.builder.addSharedLibrary("soundio", null, .{ .versioned = library_version }),
        .Static => sdk.builder.addStaticLibrary("soundio", null),
    };
    lib.setTarget(target);
    lib.linkLibC();

    lib.addIncludePath(sdkPath("/"));
    lib.addIncludePath(target_tag);

    lib.addCSourceFiles(&sources, &c_flags);

    if (os_config.jack) {
        lib.linkSystemLibrary("jack");
        lib.addCSourceFiles(&sources_jack, &c_flags);
    }
    if (os_config.pulseaudio) {
        lib.linkSystemLibrary("pulse");
        lib.addCSourceFiles(&sources_pulseaudio, &c_flags);
    }
    if (os_config.alsa) {
        lib.linkSystemLibrary("asound");
        lib.addCSourceFiles(&sources_alsa, &c_flags);
    }
    if (os_config.coreaudio) {
        lib.linkFramework("CoreAudio");
        lib.addCSourceFiles(&sources_coreaudio, &c_flags);
    }
    if (os_config.wasapi) {
        lib.linkSystemLibraryName("ole32");
        lib.addCSourceFiles(&sources_wasapi, c_flags ++ &[_][]const u8{"-DZIG_BUILD_WORKAROUND"});
    }

    return lib;
}

const sources = [_][]const u8{
    sdkPath("/src/soundio.c"),
    sdkPath("/src/util.c"),
    sdkPath("/src/os.c"),
    sdkPath("/src/dummy.c"),
    sdkPath("/src/channel_layout.c"),
    sdkPath("/src/ring_buffer.c"),
};

const sources_jack = [_][]const u8{
    sdkPath("/src/jack.c"),
};

const sources_pulseaudio = [_][]const u8{
    sdkPath("/src/pulseaudio.c"),
};

const sources_alsa = [_][]const u8{
    sdkPath("/src/alsa.c"),
};

const sources_coreaudio = [_][]const u8{
    sdkPath("/src/coreaudio.c"),
};

const sources_wasapi = [_][]const u8{
    sdkPath("/src/wasapi.c"),
};

const headers = [_][]const u8{
    sdkPath("/soundio/soundio.h"),
    sdkPath("/soundio/endian.h"),
};
