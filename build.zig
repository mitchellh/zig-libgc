const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const bdwgc_dep = b.dependency("bdwgc", .{
        .target = target,
        .optimize = optimize,
    });
    const gc = bdwgc_dep.artifact("gc");

    // lib for zig
    const lib = b.addStaticLibrary(.{
        .name = "gc",
        .root_source_file = .{ .path = "src/gc.zig" },
        .target = target,
        .optimize = optimize,
    });
    {
        var main_tests = b.addTest(.{
            .root_source_file = .{ .path = "src/gc.zig" },
            .target = target,
            .optimize = optimize,
        });
        main_tests.linkLibC();
        main_tests.addIncludePath(.{ .path = "vendor/bdwgc/include" });
        main_tests.linkLibrary(gc);

        const test_step = b.step("test", "Run library tests");
        test_step.dependOn(&main_tests.step);

        b.default_step.dependOn(&lib.step);
        b.installArtifact(lib);
    }

    const module = b.createModule(.{
        .root_source_file = .{ .path = "src/gc.zig" },
    });

    // example app
    const exe = b.addExecutable(.{
        .name = "example",
        .root_source_file = .{ .path = "example/basic.zig" },
        .target = target,
        .optimize = optimize,
    });
    {
        exe.linkLibC();
        exe.addIncludePath(.{ .path = "vendor/bdwgc/include" });
        exe.linkLibrary(gc);
        exe.root_module.addImport("gc", module);
        b.installArtifact(exe);

        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());
        if (b.args) |args| {
            run_cmd.addArgs(args);
        }

        const run_step = b.step("run_example", "run example");
        run_step.dependOn(&run_cmd.step);
    }
}
