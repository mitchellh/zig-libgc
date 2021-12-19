const std = @import("std");
const gc = @import("gc");

/// Example is a F to C conversion from zig.news. The only argument is
/// temperature in farenheight and then it outputs in celsius.
pub fn main() !void {
    // Initialize our GC
    var gc_allocator = gc.GcAllocator.init();
    defer gc_allocator.deinit();

    const args = try std.process.argsAlloc(gc_allocator.allocator());
    defer std.process.argsFree(gc_allocator.allocator(), args);

    if (args.len < 2) return error.ExpectedArgument;

    const f = try std.fmt.parseFloat(f32, args[1]);
    const c = (f - 32) * (5.0 / 9.0);

    const stdout = std.io.getStdOut().writer();
    try stdout.print("{d:.1}c\n", .{c});
}
