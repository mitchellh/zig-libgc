const std = @import("std");
const assert = std.debug.assert;
const testing = std.testing;
const mem = std.mem;
const Allocator = std.mem.Allocator;

pub const c = @cImport({
    @cInclude("gc.h");
});

pub const ALLOCATOR = Allocator{
    .ptr = undefined,
    .vtable = &gc_allocator_vtable,
};

/// Initialize libgc
pub fn init() void {
    if (c.GC_is_init_called() == 0) {
        c.GC_init();
    }
}

/// Returns the Allocator used for APIs in Zig
pub fn allocator() Allocator {
    init();
    return ALLOCATOR;
}

/// Enable or disable interior pointers.
/// If used, this must be called before the first allocator() call.
pub fn setAllInteriorPointers(enable_interior_pointers: bool) void {
    c.GC_set_all_interior_pointers(@intFromBool(enable_interior_pointers));
}

/// Returns the current heap size of used memory.
pub fn getHeapSize() u64 {
    return c.GC_get_heap_size();
}

/// Disable garbage collection.
pub fn disable() void {
    c.GC_disable();
}

/// Enables garbage collection. GC is enabled by default so this is
/// only useful if you called disable earlier.
pub fn enable() void {
    c.GC_enable();
}

// Performs a full, stop-the-world garbage collection. With leak detection
// enabled this will output any leaks as well.
pub fn collect() void {
    c.GC_gcollect();
}

/// Perform some garbage collection. Returns zero when work is done.
pub fn collectLittle() u8 {
    return @as(u8, @intCast(c.GC_collect_a_little()));
}

/// Enables leak-finding mode. See the libgc docs for more details.
pub fn setFindLeak(v: bool) void {
    return c.GC_set_find_leak(@intFromBool(v));
}

// TODO(mitchellh): there are so many more functions to add here
// from gc.h, just add em as they're useful.

/// GcAllocator is an implementation of std.mem.Allocator that uses
/// libgc under the covers. This means that all memory allocated with
/// this allocated doesn't need to be explicitly freed (but can be).
///
/// The GC is a singleton that is globally shared. Multiple GcAllocators
/// do not allocate separate pages of memory; they share the same underlying
/// pages.
///
// NOTE(mitchellh): this is basically just a copy of the standard CAllocator
// since libgc has a malloc/free-style interface. There are very slight differences
// due to API differences but overall the same.
pub const GcAllocator = struct {
    fn alloc(
        _: *anyopaque,
        len: usize,
        log2_align: u8,
        return_address: usize,
    ) ?[*]u8 {
        _ = return_address;
        const alignment = @as(usize, 1) << @as(Allocator.Log2Align, @intCast(log2_align));
        const ptr = c.GC_memalign(alignment, len + alignment - 1 + @sizeOf(usize));
        return @ptrCast(ptr);
    }

    fn resize(
        _: *anyopaque,
        buf: []u8,
        log2_buf_align: u8,
        new_len: usize,
        return_address: usize,
    ) bool {
        _ = log2_buf_align;
        _ = return_address;
        const full_len = c.GC_size(buf.ptr);
        return new_len <= full_len;
    }

    fn free(
        _: *anyopaque,
        buf: []u8,
        log2_buf_align: u8,
        return_address: usize,
    ) void {
        _ = log2_buf_align;
        _ = return_address;
        c.GC_free(buf.ptr);
    }
};

const gc_allocator_vtable = Allocator.VTable{
    .alloc = GcAllocator.alloc,
    .resize = GcAllocator.resize,
    .free = GcAllocator.free,
};

test "GcAllocator" {
    const alloc = allocator();

    try std.heap.testAllocator(alloc);
    try std.heap.testAllocatorAligned(alloc);
    try std.heap.testAllocatorLargeAlignment(alloc);
    try std.heap.testAllocatorAlignedShrink(alloc);
}

test "heap size" {
    // No garbage so should be 0
    try testing.expect(collectLittle() == 0);

    // Force a collection should work
    collect();

    try testing.expect(getHeapSize() > 0);
}
