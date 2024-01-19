const std = @import("std");
const assert = std.debug.assert;
const gc = @import("gc");
const GcAllocator = gc.GcAllocator;

pub fn main() !void {
    var alloc = gc.allocator();

    // We'll write to the terminal
    const stdout = std.io.getStdOut().writer();

    // Compare the output by enabling/disabling
    // gc.disable();

    // Allocate a bunch of stuff and never free it, outputting
    // the heap size along the way. When the GC is enabled,
    // it'll stabilize at a certain size.
    var i: u64 = 0;
    while (i < 10_000_000) : (i += 1) {
        const p: **u8 = @ptrCast(try alloc.alloc(*u8, @sizeOf(*u8)));
        const q = try alloc.alloc(u8, @sizeOf(u8));
        p.* = @ptrCast(q);
        _ = alloc.resize(q, 2 * @sizeOf(u8));
        if (i % 100_000 == 0) {
            const heap = gc.getHeapSize();
            try stdout.print("heap size: {d}\n", .{heap});
        }
    }
}
