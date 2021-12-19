const std = @import("std");
const assert = std.debug.assert;
const GcAllocator = @import("gc").GcAllocator;

/// Example is a F to C conversion from zig.news. The only argument is
/// temperature in farenheight and then it outputs in celsius.
pub fn main() !void {
    var alloc = GcAllocator.allocator();

    // We'll write to the terminal
    const stdout = std.io.getStdOut().writer();

    // Compare the output by enabling/disabling
    // GcAllocator.disable();

    // Allocate a bunch of stuff and never free it, outputting
    // the heap size along the way. When the GC is enabled,
    // it'll stabilize at a certain size.
    var i: u64 = 0;
    while (i < 10_000_000) : (i += 1) {
        var p = @ptrCast(**u8, try alloc.alloc(*u8, @sizeOf(*u8)));
        var q = try alloc.alloc(u8, @sizeOf(u8));
        p.* = @ptrCast(*u8, alloc.resize(q, 2 * @sizeOf(u8)).?);
        if (i % 100_000 == 0) {
            const heap = GcAllocator.getHeapSize();
            try stdout.print("heap size: {d}\n", .{heap});
        }
    }
}
