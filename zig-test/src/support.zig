const std = @import("std");
const testing = std.testing;
const libc_io = @cImport({
    @cInclude("stdio.h");
});

pub const c = @cImport({
    @cInclude("luna_test.h");
    @cInclude("luna_runtime.h");
    @cInclude("luna_error.h");
});

pub fn cString(bytes: anytype) [:0]const u8 {
    return std.mem.sliceTo(@as([*:0]const u8, @ptrCast(bytes)), 0);
}

pub fn tokenLexeme(token: c.Token) [:0]const u8 {
    if (token.lexeme != null) return cString(token.lexeme);
    return cString(&token.ibuf);
}

pub fn runtimeInit(runtime: *c.LunaRuntime) !void {
    try testing.expectEqual(@as(c_int, 1), c.luna_runtime_init(runtime, 0));
}

pub fn loadFixture(path: []const u8) ![:0]u8 {
    const c_path = try testing.allocator.allocSentinel(u8, path.len, 0);
    defer testing.allocator.free(c_path);
    @memcpy(c_path[0..path.len], path);

    const file = libc_io.fopen(c_path.ptr, "rb");
    if (file == null) return error.FixtureOpenFailed;
    defer _ = libc_io.fclose(file);

    if (libc_io.fseek(file, 0, libc_io.SEEK_END) != 0) return error.FixtureSeekFailed;
    const size_signed = libc_io.ftell(file);
    if (size_signed < 0) return error.FixtureTellFailed;
    if (libc_io.fseek(file, 0, libc_io.SEEK_SET) != 0) return error.FixtureSeekFailed;

    const size = @as(usize, @intCast(size_signed));
    const out = try testing.allocator.allocSentinel(u8, size, 0);
    const read_count = libc_io.fread(out.ptr, 1, size, file);
    if (read_count != size) {
        testing.allocator.free(out);
        return error.FixtureReadFailed;
    }

    return out;
}

pub fn expectRunSuccess(runtime: *c.LunaRuntime, source: [*:0]const u8, filename: [*:0]const u8, out_program: *?*c.AstNode) !void {
    try testing.expectEqual(@as(c_int, 1), c.luna_run_source(runtime, source, filename, out_program));
}

pub fn expectGlobalInt(runtime: *c.LunaRuntime, name: [*:0]const u8, expected: c_longlong) !void {
    const value = c.luna_runtime_get_global(runtime, name);
    try testing.expect(value != null);
    try testing.expectEqual(@as(c_uint, c.VAL_INT), c.luna_value_type_of(value));
    try testing.expectEqual(expected, c.luna_value_as_int(value));
}