const Builder = @import("std").build.Builder;

pub fn build(b: *Builder) void {
    const mode = b.standardReleaseOptions();

    // libgc
    const gc = b.addStaticLibrary("gc", null);
    {
        // TODO(mitchellh): support more complex features that are usually on
        // with libgc like threading, parallelization, etc.
        const cflags = [_][]const u8{};
        const libgc_srcs = [_][]const u8{
            "alloc.c",    "reclaim.c", "allchblk.c", "misc.c",     "mach_dep.c", "os_dep.c",
            "mark_rts.c", "headers.c", "mark.c",     "obj_map.c",  "blacklst.c", "finalize.c",
            "new_hblk.c", "dbg_mlc.c", "malloc.c",   "dyn_load.c", "typd_mlc.c", "ptr_chck.c",
            "mallocx.c",
        };

        gc.setBuildMode(mode);
        gc.linkLibC();
        gc.addIncludeDir("vendor/bdwgc/include");
        inline for (libgc_srcs) |src| {
            gc.addCSourceFile("vendor/bdwgc/" ++ src, &cflags);
        }

        const gc_step = b.step("libgc", "build libgc");
        gc_step.dependOn(&gc.step);
    }

    // lib for zig
    const lib = b.addStaticLibrary("gc", "src/gc.zig");
    {
        lib.setBuildMode(mode);

        var main_tests = b.addTest("src/gc.zig");
        main_tests.setBuildMode(mode);
        main_tests.linkLibC();
        main_tests.addIncludeDir("vendor/bdwgc/include");
        main_tests.linkLibrary(gc);

        const test_step = b.step("test", "Run library tests");
        test_step.dependOn(&main_tests.step);

        b.default_step.dependOn(&lib.step);
        b.installArtifact(lib);
    }

    // example app
    const exe = b.addExecutable("example", "example/basic.zig");
    {
        exe.linkLibC();
        exe.addIncludeDir("vendor/bdwgc/include");
        exe.linkLibrary(gc);
        exe.addPackage(.{
            .name = "gc",
            .path = .{ .path = "src/gc.zig" },
        });
        exe.install();

        const install_cmd = b.addInstallArtifact(exe);

        const run_cmd = exe.run();
        run_cmd.step.dependOn(&install_cmd.step);
        if (b.args) |args| {
            run_cmd.addArgs(args);
        }

        const run_step = b.step("run_example", "run example");
        run_step.dependOn(&run_cmd.step);
    }
}
