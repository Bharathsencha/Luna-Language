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
    const m_size: usize = 300;
    const count = m_size * m_size;

    const A = try allocator.alloc(f64, count);
    defer allocator.free(A);
    const B = try allocator.alloc(f64, count);
    defer allocator.free(B);
    const C = try allocator.alloc(f64, count);
    defer allocator.free(C);

    @memset(A, 1.5);
    @memset(B, 2.5);
    @memset(C, 0.0);

    for (0..m_size) |i| {
        for (0..m_size) |k| {
            const r = A[i * m_size + k];
            const brow = B[k * m_size .. (k + 1) * m_size];
            const crow = C[i * m_size .. (i + 1) * m_size];
            for (0..m_size) |j| {
                crow[j] += r * brow[j];
            }
        }
    }

    @memset(C, 0.0);
    const start = nowNs();

    for (0..m_size) |i| {
        for (0..m_size) |k| {
            const r = A[i * m_size + k];
            const brow = B[k * m_size .. (k + 1) * m_size];
            const crow = C[i * m_size .. (i + 1) * m_size];
            for (0..m_size) |j| {
                crow[j] += r * brow[j];
            }
        }
    }

    const seconds = @as(f64, @floatFromInt(nowNs() - start)) / 1_000_000_000.0;
    std.debug.print("Zig Contiguous Matrix Time: {d:.6} seconds\n", .{seconds});
}