const std = @import("std");

const gc = @cImport({
    @cInclude("gc.h");
});

pub fn main() !void {
    gc.GC_init();
    defer gc.GC_deinit();

    const stdout = std.io.getStdOut().writer();
    try stdout.print("Hello, {s}!\n", .{"world"});
}
