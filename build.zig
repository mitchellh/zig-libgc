const Builder = @import("std").build.Builder;

pub fn build(b: *Builder) void {
    const cflags = [_][]const u8{};
    const libgc_srcs = [_][]const u8{
        "alloc.c",    "reclaim.c", "allchblk.c", "misc.c",     "mach_dep.c", "os_dep.c",
        "mark_rts.c", "headers.c", "mark.c",     "obj_map.c",  "blacklst.c", "finalize.c",
        "new_hblk.c", "dbg_mlc.c", "malloc.c",   "dyn_load.c", "typd_mlc.c", "ptr_chck.c",
        "mallocx.c",
    };

    const mode = b.standardReleaseOptions();
    const lib = b.addStaticLibrary("gc", null);
    lib.setBuildMode(mode);
    lib.linkLibC();
    lib.addIncludeDir("lib/bdwgc/include");
    inline for (libgc_srcs) |src| {
        lib.addCSourceFile("lib/bdwgc/" ++ src, &cflags);
    }

    b.default_step.dependOn(&lib.step);
    b.installArtifact(lib);
}
