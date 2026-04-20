const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "parser error capture keeps exact line and message" {
    const source = try support.loadFixture("cases/error/parser_missing_name.lu");
    defer testing.allocator.free(source);
    c.error_set_quiet(1);
    defer c.error_set_quiet(0);
    const program = c.luna_parse_source(source, "parse_error_case.lu");
    defer if (program != null) c.ast_release(program);

    var err: c.LunaErrorInfo = undefined;

    try testing.expect(program == null);
    try testing.expectEqual(@as(c_int, 1), c.error_get_last(&err));
    try testing.expectEqual(@as(c_uint, c.ERR_SYNTAX), err.type);
    try testing.expectEqual(@as(c_int, 1), err.line);
    try testing.expect(std.mem.indexOf(u8, support.cString(&err.message), "Expected variable name") != null);
}

test "runtime error capture reports failing line for interpreter regressions" {
    const source = try support.loadFixture("cases/error/runtime_missing.lu");
    defer testing.allocator.free(source);
    var runtime: c.LunaRuntime = undefined;
    var program: ?*c.AstNode = null;
    var err: c.LunaErrorInfo = undefined;

    c.error_set_quiet(1);
    defer c.error_set_quiet(0);
    try support.runtimeInit(&runtime);
    defer c.luna_runtime_shutdown(&runtime);
    defer if (program != null) c.ast_release(program);

    try testing.expectEqual(@as(c_int, 0), c.luna_run_source(&runtime, source, "runtime_error_case.lu", &program));
    try testing.expectEqual(@as(c_int, 1), c.error_get_last(&err));
    try testing.expectEqual(@as(c_uint, c.ERR_NAME), err.type);
    try testing.expectEqual(@as(c_int, 1), err.line);
    try testing.expect(std.mem.indexOf(u8, support.cString(&err.message), "Variable 'missing' is not defined") != null);
}