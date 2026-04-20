const std = @import("std");
const c = @cImport({
    @cInclude("time.h");
});

fn nowNs() u64 {
    var ts: c.struct_timespec = undefined;
    _ = c.clock_gettime(c.CLOCK_MONOTONIC, &ts);
    return @as(u64, @intCast(ts.tv_sec)) * 1_000_000_000 + @as(u64, @intCast(ts.tv_nsec));
}

pub fn main() !void {
    const allocator = std.heap.page_allocator;
    const size: usize = 1_000_000;

    const A = try allocator.alloc(f64, size);
    defer allocator.free(A);
    const B = try allocator.alloc(f64, size);
    defer allocator.free(B);
    const C = try allocator.alloc(f64, size);
    defer allocator.free(C);

    @memset(A, 1.5);
    @memset(B, 2.5);
    @memset(C, 0.0);

    const start = nowNs();

    for (0..size) |i| {
        C[i] = A[i] * B[i];
    }

    const seconds = @as(f64, @floatFromInt(nowNs() - start)) / 1_000_000_000.0;
    std.debug.print("Zig Native Vector Time: {d:.4} seconds\n", .{seconds});
}