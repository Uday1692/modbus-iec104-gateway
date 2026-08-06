# Zig Build Script for Modbus-IEC104 Gateway
# Install Zig from: https://ziglang.org/

const std = @import("std");
const Build = std.Build;

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "gateway",
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
    });

    exe.addCSourceFiles(.{
        .files = &.{
            "src/main.c",
            "src/modbus_master.c",
            "src/iec104_protocol.c",
            "src/iec104_server.c",
            "src/config_parser.c",
            "src/gateway.c",
        },
        .flags = &.{
            "-std=c99",
            "-Wall",
            "-Wextra",
        },
    });

    exe.addIncludePath(b.path("src"));
    exe.linkSystemLibrary("modbus");
    exe.linkSystemLibrary("pthread");
    exe.linkLibC();

    b.installArtifact(exe);
}
