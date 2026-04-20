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

    var A = try allocator.alloc([]f64, m_size);
    defer allocator.free(A);
    var B = try allocator.alloc([]f64, m_size);
    defer allocator.free(B);
    var C = try allocator.alloc([]f64, m_size);
    defer allocator.free(C);

    for (0..m_size) |i| {
        A[i] = try allocator.alloc(f64, m_size);
        B[i] = try allocator.alloc(f64, m_size);
        C[i] = try allocator.alloc(f64, m_size);
        defer allocator.free(A[i]);
        defer allocator.free(B[i]);
        defer allocator.free(C[i]);

        for (0..m_size) |j| {
            A[i][j] = 1.5;
            B[i][j] = 2.5;
            C[i][j] = 0.0;
        }
    }

    const start = nowNs();

    for (0..m_size) |i| {
        const Ai = A[i];
        const Ci = C[i];
        for (0..m_size) |k| {
            const r = Ai[k];
            const Bk = B[k];
            for (0..m_size) |j| {
                Ci[j] += r * Bk[j];
            }
        }
    }

    const seconds = @as(f64, @floatFromInt(nowNs() - start)) / 1_000_000_000.0;
    std.debug.print("Zig Native Matrix Time: {d:.4} seconds\n", .{seconds});
}