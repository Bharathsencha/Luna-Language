const std = @import("std");
const c = @cImport({
    @cInclude("time.h");
});

fn nowNs() u64 {
    var ts: c.struct_timespec = undefined;
    _ = c.clock_gettime(c.CLOCK_MONOTONIC, &ts);
    return @as(u64, @intCast(ts.tv_sec)) * 1_000_000_000 + @as(u64, @intCast(ts.tv_nsec));
}

const size: usize = 1_000_000;
const global_var: i64 = 42;

fn testLookups() void {
    const start = nowNs();
    const local_var: i64 = 10;
    var sum_val: i64 = 0;

    for (0..size) |_| {
        sum_val += local_var + global_var;
    }

    const seconds = @as(f64, @floatFromInt(nowNs() - start)) / 1_000_000_000.0;
    std.debug.print("Zig Env Lookup Time: {d:.4} seconds\n", .{seconds});
    std.mem.doNotOptimizeAway(sum_val);
}

pub fn main() void {
    testLookups();
}