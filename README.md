# zig-libgc

This library implements a Zig allocator that uses the
[Boehm-Demers-Weiser conservative Garbage Collector (libgc, bdwgc, boehm-gc)](https://github.com/ivmai/bdwgc).
Values allocated within this allocator do not need to be explicitly
freed.

**Should I use a GC with Zig?** _Probably not_, but it depends on your
use case. A garbage collector is nice in certain scenarios, can make
implementing certain data structures easier (particularly immutable ones),
etc. The nice thing about Zig is you can choose what you want and don't
want in the garbage collector.

## Example

```zig
const std = @import("std");
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
        // This is all really ugly but its not idiomatic Zig code so
        // just take this at face value. We're doing weird stuff here
        // to show that we're collecting garbage.
        var p = @ptrCast(**u8, try alloc.alloc(*u8, @sizeOf(*u8)));
        var q = try alloc.alloc(u8, @sizeOf(u8));
        p.* = @ptrCast(*u8, alloc.resize(q, 2 * @sizeOf(u8)).?);

        if (i % 100_000 == 0) {
            const heap = gc.getHeapSize();
            try stdout.print("heap size: {d}\n", .{heap});
        }
    }
}
```
