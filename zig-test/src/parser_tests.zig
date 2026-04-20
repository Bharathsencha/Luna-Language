const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "parser builds let AST with stable shape" {
    const source = try support.loadFixture("cases/parser/let_number.lu");
    defer testing.allocator.free(source);
    const program = c.luna_parse_source(source, "parser_case.lu");
    defer if (program != null) c.ast_release(program);

    try testing.expect(program != null);
    const root = program.?;
    try testing.expectEqual(@as(c_uint, c.NODE_BLOCK), c.luna_ast_kind(root));
    try testing.expectEqual(@as(c_int, 1), c.luna_ast_block_count(root));

    const stmt = c.luna_ast_block_item(root, 0);
    try testing.expect(stmt != null);
    try testing.expectEqual(@as(c_uint, c.NODE_LET), c.luna_ast_kind(stmt));
    try testing.expectEqual(@as(c_int, 1), c.luna_ast_line(stmt));

    const expr = c.luna_ast_let_expr(stmt);
    try testing.expect(expr != null);
    try testing.expectEqual(@as(c_uint, c.NODE_NUMBER), c.luna_ast_kind(expr));
}

test "parser keeps multiple top level statements in order" {
    const source = try support.loadFixture("cases/parser/multi_top_level.lu");
    defer testing.allocator.free(source);
    const program = c.luna_parse_source(source, "parser_multi_case.lu");
    defer if (program != null) c.ast_release(program);

    try testing.expect(program != null);
    const root = program.?;
    try testing.expectEqual(@as(c_int, 2), c.luna_ast_block_count(root));
    try testing.expectEqual(@as(c_int, 1), c.luna_ast_line(c.luna_ast_block_item(root, 0)));
    try testing.expectEqual(@as(c_int, 2), c.luna_ast_line(c.luna_ast_block_item(root, 1)));
}

test "parser keeps binary precedence stable inside let expressions" {
    const source = try support.loadFixture("cases/parser/let_precedence.lu");
    defer testing.allocator.free(source);
    const program = c.luna_parse_source(source, "parser_precedence_case.lu");
    defer if (program != null) c.ast_release(program);

    try testing.expect(program != null);
    const root = program.?;
    const stmt = c.luna_ast_block_item(root, 0);
    try testing.expect(stmt != null);

    const expr = c.luna_ast_let_expr(stmt);
    try testing.expect(expr != null);
    try testing.expectEqual(@as(c_uint, c.NODE_BINOP), c.luna_ast_kind(expr));
    try testing.expectEqual(@as(c_uint, c.OP_ADD), c.luna_ast_binop_op(expr));

    const left = c.luna_ast_binop_left(expr);
    const right = c.luna_ast_binop_right(expr);
    try testing.expect(left != null);
    try testing.expect(right != null);
    try testing.expectEqual(@as(c_uint, c.NODE_IDENT), c.luna_ast_kind(left));
    try testing.expectEqual(@as(c_uint, c.NODE_BINOP), c.luna_ast_kind(right));
    try testing.expectEqual(@as(c_uint, c.OP_MUL), c.luna_ast_binop_op(right));
}