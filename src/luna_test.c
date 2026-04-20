// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdlib.h>
#include <string.h>
#include "luna_test.h"
#include "lexer.h"
#include "mystr.h"

static Token token_clone(Token token) {
    Token copy = token;
    if (token.lexeme) {
        copy.lexeme = my_strdup(token.lexeme);
    }
    return copy;
}

int luna_lex_source(const char *source, LunaTokenBuffer *out_tokens) {
    Lexer lexer;
    size_t capacity = 0;
    Token token;

    if (!source || !out_tokens) return 0;

    out_tokens->items = NULL;
    out_tokens->count = 0;
    lexer = lexer_create(source);

    while (1) {
        Token *grown;
        token = lexer_next(&lexer);

        if (out_tokens->count == capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            grown = realloc(out_tokens->items, sizeof(Token) * capacity);
            if (!grown) {
                free_token(&token);
                luna_token_buffer_free(out_tokens);
                return 0;
            }
            out_tokens->items = grown;
        }

        out_tokens->items[out_tokens->count++] = token_clone(token);
        free_token(&token);

        if (out_tokens->items[out_tokens->count - 1].type == T_EOF) {
            break;
        }
    }

    return 1;
}

void luna_token_buffer_free(LunaTokenBuffer *buffer) {
    size_t i;

    if (!buffer || !buffer->items) return;

    for (i = 0; i < buffer->count; i++) {
        free_token(&buffer->items[i]);
    }
    free(buffer->items);
    buffer->items = NULL;
    buffer->count = 0;
}

NodeKind luna_ast_kind(const AstNode *node) {
    return node ? node->kind : NODE_BLOCK;
}

int luna_ast_line(const AstNode *node) {
    return node ? node->line : 0;
}

int luna_ast_block_count(const AstNode *node) {
    if (!node || (node->kind != NODE_BLOCK && node->kind != NODE_GROUP)) return 0;
    return node->block.items.count;
}

AstNode *luna_ast_block_item(const AstNode *node, int index) {
    if (!node || (node->kind != NODE_BLOCK && node->kind != NODE_GROUP)) return NULL;
    if (index < 0 || index >= node->block.items.count) return NULL;
    return node->block.items.items[index];
}

AstNode *luna_ast_let_expr(const AstNode *node) {
    if (!node || node->kind != NODE_LET) return NULL;
    return node->let.expr;
}

BinOpKind luna_ast_binop_op(const AstNode *node) {
    return node ? node->binop.op : OP_ADD;
}

AstNode *luna_ast_binop_left(const AstNode *node) {
    if (!node || node->kind != NODE_BINOP) return NULL;
    return node->binop.left;
}

AstNode *luna_ast_binop_right(const AstNode *node) {
    if (!node || node->kind != NODE_BINOP) return NULL;
    return node->binop.right;
}

ValueType luna_value_type_of(const Value *value) {
    return value ? value->type : VAL_NULL;
}

long long luna_value_as_int(const Value *value) {
    if (!value || value->type != VAL_INT) return 0;
    return value->i;
}