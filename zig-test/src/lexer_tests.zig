const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "lexer emits stable token sequence with lines" {
    const source = try support.loadFixture("cases/lexer/basic.lu");
    defer testing.allocator.free(source);

    var tokens: c.LunaTokenBuffer = .{ .items = null, .count = 0 };
    defer c.luna_token_buffer_free(&tokens);

    try testing.expectEqual(@as(c_int, 1), c.luna_lex_source(source, &tokens));
    try testing.expectEqual(@as(usize, 11), tokens.count);

    const items = tokens.items.?;
    try testing.expectEqual(@as(c_uint, c.T_LET), items[0].type);
    try testing.expectEqual(@as(c_uint, c.T_IDENT), items[1].type);
    try testing.expectEqual(@as(c_uint, c.T_EQ), items[2].type);
    try testing.expectEqual(@as(c_uint, c.T_NUMBER), items[3].type);
    try testing.expectEqual(@as(c_uint, c.T_NEWLINE), items[4].type);
    try testing.expectEqual(@as(c_uint, c.T_PRINT), items[5].type);
    try testing.expectEqual(@as(c_uint, c.T_LPAREN), items[6].type);
    try testing.expectEqual(@as(c_uint, c.T_IDENT), items[7].type);
    try testing.expectEqual(@as(c_uint, c.T_RPAREN), items[8].type);
    try testing.expectEqual(@as(c_uint, c.T_NEWLINE), items[9].type);
    try testing.expectEqual(@as(c_uint, c.T_EOF), items[10].type);
    try testing.expectEqual(@as(c_int, 2), items[5].line);
}

test "lexer skips comments but preserves statement newlines" {
    const source = try support.loadFixture("cases/lexer/comments.lu");
    defer testing.allocator.free(source);

    var tokens: c.LunaTokenBuffer = .{ .items = null, .count = 0 };
    defer c.luna_token_buffer_free(&tokens);

    try testing.expectEqual(@as(c_int, 1), c.luna_lex_source(source, &tokens));

    const items = tokens.items.?;
    try testing.expectEqual(@as(c_uint, c.T_LET), items[0].type);
    try testing.expectEqual(@as(c_uint, c.T_NEWLINE), items[4].type);
    try testing.expectEqual(@as(c_int, 1), items[4].line);
    try testing.expectEqual(@as(c_uint, c.T_NEWLINE), items[5].type);
    try testing.expectEqual(@as(c_int, 2), items[5].line);
    try testing.expectEqual(@as(c_uint, c.T_LET), items[6].type);
    try testing.expectEqual(@as(c_int, 3), items[6].line);
}

test "lexer keeps numeric literal lexeme stable for fixtures" {
    const source = try support.loadFixture("cases/lexer/basic.lu");
    defer testing.allocator.free(source);

    var tokens: c.LunaTokenBuffer = .{ .items = null, .count = 0 };
    defer c.luna_token_buffer_free(&tokens);

    try testing.expectEqual(@as(c_int, 1), c.luna_lex_source(source, &tokens));

    const items = tokens.items.?;
    try testing.expectEqualStrings("x", support.tokenLexeme(items[1]));
    try testing.expectEqualStrings("42", support.tokenLexeme(items[3]));
}