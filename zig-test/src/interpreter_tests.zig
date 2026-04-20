const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "interpreter updates global bindings for regression checks" {
    const source =
        \\let x = 41
        \\x = x + 1
    ;

    var runtime: c.LunaRuntime = undefined;
    var program: ?*c.AstNode = null;

    try support.runtimeInit(&runtime);
    defer c.luna_runtime_shutdown(&runtime);
    defer if (program != null) c.ast_release(program);

    try support.expectRunSuccess(&runtime, source, "interpreter_case.lu", &program);
    try support.expectGlobalInt(&runtime, "x", 42);
}

test "interpreter executes branches and dependent assignments" {
    const source = try support.loadFixture("cases/interpreter/assign_branch.lu");
    defer testing.allocator.free(source);

    var runtime: c.LunaRuntime = undefined;
    var program: ?*c.AstNode = null;

    try support.runtimeInit(&runtime);
    defer c.luna_runtime_shutdown(&runtime);
    defer if (program != null) c.ast_release(program);

    try support.expectRunSuccess(&runtime, source, "interpreter_branch_case.lu", &program);
    try support.expectGlobalInt(&runtime, "score", 10);
    try support.expectGlobalInt(&runtime, "doubled", 20);
}

test "interpreter preserves function results from fixtures" {
    const source = try support.loadFixture("cases/interpreter/while_accumulate.lu");
    defer testing.allocator.free(source);

    var runtime: c.LunaRuntime = undefined;
    var program: ?*c.AstNode = null;

    try support.runtimeInit(&runtime);
    defer c.luna_runtime_shutdown(&runtime);
    defer if (program != null) c.ast_release(program);

    try support.expectRunSuccess(&runtime, source, "interpreter_loop_case.lu", &program);
    try support.expectGlobalInt(&runtime, "total", 10);
    try support.expectGlobalInt(&runtime, "i", 5);
}