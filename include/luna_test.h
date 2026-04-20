// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_TEST_H
#define LUNA_TEST_H

#include <stddef.h>
#include "ast.h"
#include "luna_runtime.h"
#include "token.h"

typedef struct {
    Token *items;
    size_t count;
} LunaTokenBuffer;

int luna_lex_source(const char *source, LunaTokenBuffer *out_tokens);
void luna_token_buffer_free(LunaTokenBuffer *buffer);

NodeKind luna_ast_kind(const AstNode *node);
int luna_ast_line(const AstNode *node);
int luna_ast_block_count(const AstNode *node);
AstNode *luna_ast_block_item(const AstNode *node, int index);
AstNode *luna_ast_let_expr(const AstNode *node);
BinOpKind luna_ast_binop_op(const AstNode *node);
AstNode *luna_ast_binop_left(const AstNode *node);
AstNode *luna_ast_binop_right(const AstNode *node);

ValueType luna_value_type_of(const Value *value);
long long luna_value_as_int(const Value *value);

const char *ast_node_kind_name(NodeKind kind);
const char *ast_binop_kind_name(BinOpKind kind);

#endif