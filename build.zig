# Zig Build Script for Modbus-IEC104 Gateway
# Install Zig from: https://ziglang.org/

const std = @import("std");
const Build = std.Build;

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Create executable
    const exe = b.addExecutable(.{
        .name = "gateway",
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
    });

    // Add source files
    exe.addCSourceFiles(&.{
        "src/main.c",
        "src/modbus_master.c",
        "src/iec104_server.c",
        "src/gateway.c",
    }, &.{
        "-std=c99",
        "-Wall",
        "-Wextra",
    });

    // Add include directories
    exe.addIncludePath(.{ .path = "src" });

    // Link libraries
    exe.linkSystemLibrary("modbus");
    exe.linkSystemLibrary("pthread");
    exe.linkLibC();

    b.installArtifact(exe);
}
