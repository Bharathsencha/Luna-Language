// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "ast.h"
#include "token.h"
#include "mystr.h"
#include "luna_error.h"
#include "intern.h"

static void advance(Parser *p) {
    if (p->had_error) return; // Stop advancing if we have an error
    p->cur = lexer_next(&p->lx);
    p->has_cur = 1;
}

static int check(Parser *p, TokenType type) {
    if (p->had_error || !p->has_cur) {
        return 0;
    }
    return p->cur.type == type;
}

static int match(Parser *p, TokenType type) {
    if (check(p, type)) {
        free_token(&p->cur);
        advance(p);
        return 1;
    }
    return 0;
}

static void consume(Parser *p, TokenType type, const char *err) {
    if (p->had_error) return; // Propagate error

    if (check(p, type)) {
        free_token(&p->cur);
        advance(p);
    } else {
        // Report error and set flag instead of exit(1)
        const char *expected = token_name(type);
        const char *found = token_name(p->cur.type);
        const char *suggestion = suggest_for_unexpected_token(found, expected);
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col, err, suggestion);
        p->had_error = 1;
    }
}

void parser_init(Parser *p, const char *source) {
    p->lx = lexer_create(source);
    p->inside_function = 0;
    p->had_error = 0; // Initialize error flag
    advance(p);
}

void parser_close(Parser *p) {
    if (p->has_cur) {
        free_token(&p->cur);
        p->has_cur = 0;
    }
}

static AstNode *expression(Parser *p);
static AstNode *statement(Parser *p);
static void block(Parser *p, NodeList *list);
static AstNode *function_def(Parser *p);
static AstNode *function_literal(Parser *p, const char *name, int line);
static AstNode *data_def(Parser *p, int line, int is_template);
static AstNode *bloc_def(Parser *p, int line);
static int lint_unsafe_stmt(Parser *p, AstNode *n, int in_unsafe, int in_conditional);
static int lint_unsafe_expr(Parser *p, AstNode *n, int in_unsafe, int in_conditional);

static void mark_stmt_exported(AstNode *n) {
    if (!n) return;
    if (n->kind == NODE_LET) {
        n->let.is_export = 1;
        return;
    }
    if (n->kind == NODE_FUNC_DEF) {
        n->funcdef.is_export = 1;
        return;
    }
    if (n->kind == NODE_DATA_DEF) {
        n->data_def.is_export = 1;
        return;
    }
    if (n->kind == NODE_BLOC_DEF) {
        n->bloc_def.is_export = 1;
        return;
    }
    if (n->kind == NODE_GROUP) {
        for (int i = 0; i < n->block.items.count; i++) {
            mark_stmt_exported(n->block.items.items[i]);
        }
    }
}


static char *slice_text(const char *src, size_t start, size_t len) {
    char *out = malloc(len + 1);
    memcpy(out, src + start, len);
    out[len] = '\0';
    return out;
}

static AstNode *parse_inline_expr(const char *src, int line) {
    Parser inner;
    parser_init(&inner, src);
    AstNode *expr = expression(&inner);
    while (match(&inner, T_NEWLINE));
    if (!inner.had_error && !check(&inner, T_EOF)) {
        error_report_with_context(ERR_SYNTAX, line, 0,
            "Invalid expression inside string interpolation",
            "Use a valid Luna expression inside { ... }");
        inner.had_error = 1;
    }
    parser_close(&inner);
    if (inner.had_error) return NULL;
    return expr;
}

static AstNode *parse_string_literal_or_template(Parser *p, int line) {
    const char *raw = token_str(&p->cur);
    char *s = my_strdup(raw);
    size_t len = strlen(s);
    free_token(&p->cur);
    advance(p);

    /* Check for interpolation markers: { followed by non-} */
    int has_interp = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '{' && i + 1 < len && s[i + 1] != '}') {
            has_interp = 1;
            break;
        }
    }
    if (!has_interp) {
        AstNode *n = ast_string(s, line);
        free(s);
        return n;
    }

    const char **chunks = NULL;
    AstNode **exprs = NULL;
    int expr_count = 0;
    int expr_cap = 0;
    size_t chunk_start = 0;

    for (size_t i = 0; i < len; i++) {
        if (s[i] != '{') continue;
        if (i + 1 < len && s[i + 1] == '}') {
            continue;
        }
        size_t expr_start = i + 1;
        int brace_depth = 1;
        size_t j = expr_start;
        for (; j < len; j++) {
            if (s[j] == '{') brace_depth++;
            else if (s[j] == '}') {
                brace_depth--;
                if (brace_depth == 0) break;
            }
        }
        if (j >= len) {
            error_report_with_context(ERR_SYNTAX, line, 0,
                "Unclosed string interpolation expression",
                "Close the interpolation with '}'");
            p->had_error = 1;
            free(chunks);
            free(exprs);
            free(s);
            return NULL;
        }

        if (expr_count >= expr_cap) {
            int next_cap = expr_cap == 0 ? 4 : expr_cap * 2;
            const char **grown_chunks = malloc(sizeof(const char *) * (size_t)(next_cap + 1));
            AstNode **grown_exprs = malloc(sizeof(AstNode *) * (size_t)next_cap);
            if (!grown_chunks || !grown_exprs) {
                free(grown_chunks);
                free(grown_exprs);
                for (int k = 0; k < expr_count; k++) free((void *)chunks[k]);
                free(chunks);
                free(exprs);
                free(s);
                p->had_error = 1;
                return NULL;
            }
            if (expr_count > 0) {
                memcpy(grown_chunks, chunks, sizeof(const char *) * (size_t)expr_count);
                memcpy(grown_exprs, exprs, sizeof(AstNode *) * (size_t)expr_count);
            }
            free(chunks);
            free(exprs);
            chunks = grown_chunks;
            exprs = grown_exprs;
            expr_cap = next_cap;
        }
        chunks[expr_count] = slice_text(s, chunk_start, i - chunk_start);
        char *expr_src = slice_text(s, expr_start, j - expr_start);
        AstNode *expr = parse_inline_expr(expr_src, line);
        free(expr_src);
        if (!expr) {
            for (int k = 0; k <= expr_count; k++) free((void *)chunks[k]);
            free(chunks);
            free(exprs);
            free(s);
            p->had_error = 1;
            return NULL;
        }
        exprs[expr_count] = expr;
        expr_count++;
        chunk_start = j + 1;
        i = j;
    }

    chunks[expr_count] = slice_text(s, chunk_start, len - chunk_start);
    AstNode *templ = ast_template(chunks, exprs, expr_count, line);
    for (int i = 0; i < expr_count + 1; i++) free((void *)chunks[i]);
    free(chunks);
    free(exprs);
    free(s);
    return templ;
}

static int lint_unsafe_list(Parser *p, NodeList *list, int in_unsafe, int in_conditional) {
    for (int i = 0; i < list->count; i++) {
        if (!lint_unsafe_stmt(p, list->items[i], in_unsafe, in_conditional)) {
            return 0;
        }
    }
    return 1;
}

static int is_call_named(AstNode *n, const char *name) {
    return n &&
           n->kind == NODE_CALL &&
           n->call.callee &&
           n->call.callee->kind == NODE_IDENT &&
           strcmp(n->call.callee->ident.name, name) == 0;
}

static int report_conditional_free(Parser *p, int line) {
    error_report_with_context(
        ERR_STATIC,
        line,
        0,
        "free() may not appear inside conditional control flow",
        "Move free(ptr) out of the branch/loop, or use defer(ptr) instead");
    p->had_error = 1;
    return 0;
}

static int lint_unsafe_expr(Parser *p, AstNode *n, int in_unsafe, int in_conditional) {
    if (!n || p->had_error) return !p->had_error;

    if (in_unsafe && in_conditional && is_call_named(n, "free")) {
        return report_conditional_free(p, n->line);
    }

    switch (n->kind) {
        case NODE_LIST:
            return lint_unsafe_list(p, &n->list.items, in_unsafe, in_conditional);
        case NODE_MAP:
            for (int i = 0; i < n->map.count; i++) {
                if (!lint_unsafe_expr(p, n->map.values[i], in_unsafe, in_conditional)) return 0;
            }
            return 1;
        case NODE_BINOP:
            return lint_unsafe_expr(p, n->binop.left, in_unsafe, in_conditional) &&
                   lint_unsafe_expr(p, n->binop.right, in_unsafe, in_conditional);
        case NODE_CALL:
            if (!lint_unsafe_expr(p, n->call.callee, in_unsafe, in_conditional)) return 0;
            for (int i = 0; i < n->call.args.count; i++) {
                if (!lint_unsafe_expr(p, n->call.args.items[i], in_unsafe, in_conditional)) return 0;
            }
            return 1;
        case NODE_INDEX:
            return lint_unsafe_expr(p, n->index.target, in_unsafe, in_conditional) &&
                   lint_unsafe_expr(p, n->index.index, in_unsafe, in_conditional);
        case NODE_FIELD:
            return lint_unsafe_expr(p, n->field.target, in_unsafe, in_conditional);
        case NODE_TYPED_INIT:
            return lint_unsafe_list(p, &n->typed_init.args, in_unsafe, in_conditional);
        case NODE_BOX_ALLOC:
            return lint_unsafe_expr(p, n->box_alloc.size, in_unsafe, in_conditional);
        case NODE_NOT:
            return lint_unsafe_expr(p, n->logic_not.expr, in_unsafe, in_conditional);
        case NODE_TEMPLATE:
            for (int i = 0; i < n->template_string.expr_count; i++) {
                if (!lint_unsafe_expr(p, n->template_string.exprs[i], in_unsafe, in_conditional)) return 0;
            }
            return 1;
        default:
            return 1;
    }
}

static int lint_unsafe_stmt(Parser *p, AstNode *n, int in_unsafe, int in_conditional) {
    if (!n || p->had_error) return !p->had_error;

    switch (n->kind) {
        case NODE_LET:
            return lint_unsafe_expr(p, n->let.expr, in_unsafe, in_conditional);
        case NODE_ASSIGN:
            return lint_unsafe_expr(p, n->assign.expr, in_unsafe, in_conditional);
        case NODE_ASSIGN_INDEX:
            return lint_unsafe_expr(p, n->assign_index.list, in_unsafe, in_conditional) &&
                   lint_unsafe_expr(p, n->assign_index.index, in_unsafe, in_conditional) &&
                   lint_unsafe_expr(p, n->assign_index.value, in_unsafe, in_conditional);
        case NODE_PRINT:
            return lint_unsafe_list(p, &n->print.args, in_unsafe, in_conditional);
        case NODE_RETURN:
            return lint_unsafe_expr(p, n->ret.expr, in_unsafe, in_conditional);
        case NODE_IF:
            return lint_unsafe_expr(p, n->ifstmt.cond, in_unsafe, in_unsafe ? 1 : in_conditional) &&
                   lint_unsafe_list(p, &n->ifstmt.then_block, in_unsafe, in_unsafe ? 1 : in_conditional) &&
                   lint_unsafe_list(p, &n->ifstmt.else_block, in_unsafe, in_unsafe ? 1 : in_conditional);
        case NODE_WHILE:
            return lint_unsafe_expr(p, n->whilestmt.cond, in_unsafe, in_unsafe ? 1 : in_conditional) &&
                   lint_unsafe_list(p, &n->whilestmt.body, in_unsafe, in_unsafe ? 1 : in_conditional);
        case NODE_FOR:
            return lint_unsafe_stmt(p, n->forstmt.init, in_unsafe, in_conditional) &&
                   lint_unsafe_expr(p, n->forstmt.cond, in_unsafe, in_unsafe ? 1 : in_conditional) &&
                   lint_unsafe_stmt(p, n->forstmt.incr, in_unsafe, in_unsafe ? 1 : in_conditional) &&
                   lint_unsafe_list(p, &n->forstmt.body, in_unsafe, in_unsafe ? 1 : in_conditional);
        case NODE_FOR_IN:
            return lint_unsafe_expr(p, n->forin.iterable, in_unsafe, in_conditional) &&
                   lint_unsafe_list(p, &n->forin.body, in_unsafe, in_unsafe ? 1 : in_conditional);
        case NODE_SWITCH:
            if (!lint_unsafe_expr(p, n->switchstmt.expr, in_unsafe, in_conditional)) return 0;
            for (int i = 0; i < n->switchstmt.cases.count; i++) {
                AstNode *case_node = n->switchstmt.cases.items[i];
                if (!lint_unsafe_expr(p, case_node->casestmt.value, in_unsafe, in_conditional)) return 0;
                if (!lint_unsafe_list(p, &case_node->casestmt.body, in_unsafe, in_unsafe ? 1 : in_conditional)) return 0;
            }
            return lint_unsafe_list(p, &n->switchstmt.default_case, in_unsafe, in_unsafe ? 1 : in_conditional);
        case NODE_BLOCK:
        case NODE_GROUP:
            return lint_unsafe_list(p, &n->block.items, in_unsafe, in_conditional);
        case NODE_UNSAFE:
            return lint_unsafe_list(p, &n->unsafe_block.body, 1, in_conditional);
        case NODE_FUNC_DEF:
            return lint_unsafe_list(p, &n->funcdef.body, in_unsafe, in_conditional);
        case NODE_DATA_DEF:
        case NODE_BLOC_DEF:
            return 1;
        default:
            return lint_unsafe_expr(p, n, in_unsafe, in_conditional);
    }
}

static AstNode *clone_lvalue(AstNode *n) {
    if (!n) return NULL;
    if (n->kind == NODE_IDENT) return ast_ident(n->ident.name, n->line);
    if (n->kind == NODE_INDEX) {
        return ast_index(clone_lvalue(n->index.target), clone_lvalue(n->index.index), n->line);
    }
    if (n->kind == NODE_FIELD) {
        return ast_field(clone_lvalue(n->field.target), n->field.field, n->line);
    }
    if (n->kind == NODE_NUMBER) return ast_number(n->number.value, n->line);
    if (n->kind == NODE_FLOAT) return ast_float(n->fnumber.value, n->line);
    if (n->kind == NODE_CHAR) return ast_char(n->character.value, n->line);
    if (n->kind == NODE_BOOL) return ast_bool(n->boolean.value, n->line);
    return NULL;
}

static AstNode *parse_declaration(Parser *p, int is_const) {
    int line = p->cur.line;
    const char **names = NULL;
    int name_count = 0;

    do {
        if (!check(p, T_IDENT)) {
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                is_const ? "Expected variable name after 'const' or ','" : "Expected variable name after 'let' or ','",
                "Variables must be identifiers (e.g., let a, b, c)");
            p->had_error = 1;
            free(names);
            return NULL;
        }

        names = realloc(names, sizeof(const char*) * (name_count + 1));
        names[name_count++] = intern_string(token_str(&p->cur));
        advance(p);
    } while (match(p, T_COMMA));

    AstNode **values = NULL;
    int val_count = 0;

    if (match(p, T_EQ)) {
        do {
            AstNode *val = expression(p);
            values = realloc(values, sizeof(AstNode*) * (val_count + 1));
            values[val_count++] = val;
        } while (match(p, T_COMMA));
    }

    if (is_const && val_count != name_count) {
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
            "const declarations require an initializer for every name",
            "Use const x = value");
        p->had_error = 1;
        free((void*)names);
        free(values);
        return NULL;
    }

    if (val_count > 0 && val_count != name_count) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Variable count (%d) does not match value count (%d)", name_count, val_count);
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col, msg,
            "Ensure you provide a value for every variable declared, or none at all.");
        p->had_error = 1;
        free((void*)names);
        free(values);
        return NULL;
    }

    NodeList lets;
    nodelist_init(&lets);
    for (int i = 0; i < name_count; i++) {
        AstNode *val = (val_count > 0) ? values[i] : NULL;
        nodelist_push(&lets, ast_let(names[i], val, is_const, line));
    }

    free((void*)names);
    free(values);
    if (lets.count == 1) return lets.items[0];
    return ast_group(lets, line);
}

static AstNode *primary(Parser *p) {
    if (p->had_error) return NULL; // Error check
    int line = p->cur.line;

    if (check(p, T_NUMBER)) {
        long long v = p->cur.number;
        free_token(&p->cur);
        advance(p);
        return ast_number(v, line);
    }
    if (check(p, T_FLOAT)) {
        double v = p->cur.fnumber;
        free_token(&p->cur);
        advance(p);
        return ast_float(v, line);
    }
    if (check(p, T_STRING)) {
        return parse_string_literal_or_template(p, line);
    }
    if (check(p, T_CHAR)) {
        char c = token_str(&p->cur)[0];
        free_token(&p->cur);
        advance(p);
        return ast_char(c, line);
    }
    if (check(p, T_TRUE)) {
        match(p, T_TRUE);
        return ast_bool(1, line);
    }
    if (check(p, T_FALSE)) {
        match(p, T_FALSE);
        return ast_bool(0, line);
    }
    if (check(p, T_IDENT)) {
        // Core Optimization: All variable identifiers are now Interned Pointers
        const char *name = intern_string(token_str(&p->cur));
        free_token(&p->cur);
        advance(p);
        return ast_ident(name, line); // No need to free, managed by intern table
    }
    if (match(p, T_BOX)) {
        consume(p, T_LBRACKET, "Expected '[' after 'box'");
        AstNode *size = expression(p);
        consume(p, T_RBRACKET, "Expected ']' after box size");
        return ast_box_alloc(size, line);
    }
    if (match(p, T_LPAREN)) {
        AstNode *expr = expression(p);
        consume(p, T_RPAREN, "Expected ')' after expression");
        return expr;
    }
    if (match(p, T_LBRACKET)) {
        NodeList items;
        nodelist_init(&items);

        while (match(p, T_NEWLINE)); // Skip leading newlines

        if (!check(p, T_RBRACKET)) {
            do {
                while (match(p, T_NEWLINE)); // Skip newlines before element
                
                AstNode *elem = expression(p);
                if (elem) nodelist_push(&items, elem);

                while (match(p, T_NEWLINE)); // Skip newlines after element

            } while (match(p, T_COMMA));
        }
        
        while (match(p, T_NEWLINE)); // Skip trailing newlines
        consume(p, T_RBRACKET, "Expected ']' at end of list");
        return ast_list(items, line);
    }
    if (match(p, T_LBRACE)) {
        const char **keys = NULL;
        AstNode **values = NULL;
        int count = 0;

        while (match(p, T_NEWLINE));
        if (!check(p, T_RBRACE)) {
            do {
                while (match(p, T_NEWLINE));
                if (!check(p, T_STRING)) {
                    error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                        "Map literal keys must be strings",
                        "Use {\"key\": value} for map literals");
                    p->had_error = 1;
                    free(keys);
                    free(values);
                    return NULL;
                }

                keys = realloc(keys, sizeof(const char*) * (count + 1));
                values = realloc(values, sizeof(AstNode*) * (count + 1));
                keys[count] = intern_string(token_str(&p->cur));
                free_token(&p->cur);
                advance(p);
                consume(p, T_COLON, "Expected ':' after map key");
                values[count] = expression(p);
                count++;
                while (match(p, T_NEWLINE));
            } while (match(p, T_COMMA));
        }
        while (match(p, T_NEWLINE));
        consume(p, T_RBRACE, "Expected '}' at end of map literal");
        AstNode *n = ast_map(keys, values, count, line);
        free(keys);
        free(values);
        return n;
    }
    if (match(p, T_FUNC)) {
        return function_literal(p, NULL, line);
    }

    // Parse input(...) expression with optional prompt string
    if (match(p, T_INPUT)) {
        consume(p, T_LPAREN, "Expected '(' after input");
        char *prompt = NULL;
        if (!check(p, T_RPAREN)) {
            if (check(p, T_STRING)) {
                prompt = my_strdup(token_str(&p->cur));
                advance(p);
            } else {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected string prompt for input",
                    "Use input(\"prompt\") to get user input with a message");
                p->had_error = 1;
                return NULL;
            }
        }
        consume(p, T_RPAREN, "Expected ')' after input");
        AstNode *n = ast_input(prompt, line);
        if (prompt) free(prompt);
        return n;
    }

    if (check(p, T_SEMICOLON)) {
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
            "Unexpected semicolon ';'",
            "fucking noob");
        p->had_error = 1;
        return NULL;
    }

    // Set error flag instead of exit(1)
    char msg[128];
    snprintf(msg, sizeof(msg), "Unexpected token '%s'", token_name(p->cur.type));
    error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col, msg, 
        "Expected an expression (number, string, variable, or '(')");
    p->had_error = 1;
    return NULL;
}

static AstNode *call_or_index(Parser *p) {
    int line = p->cur.line;
    AstNode *expr = primary(p);

    while (1) {
        if (p->had_error) return expr; // Stop if error occurred

        if (match(p, T_LPAREN)) {
            NodeList args;
            nodelist_init(&args);
            if (!check(p, T_RPAREN)) {
                do {
                    AstNode *arg = expression(p);
                    if (arg) nodelist_push(&args, arg);
                } while (match(p, T_COMMA));
            }
            consume(p, T_RPAREN, "Expected ')' after arguments");

            expr = ast_call(expr, args, line);
        } else if (expr && expr->kind == NODE_IDENT && match(p, T_LBRACE)) {
            NodeList args;
            nodelist_init(&args);
            while (match(p, T_NEWLINE));
            if (!check(p, T_RBRACE)) {
                do {
                    while (match(p, T_NEWLINE));
                    AstNode *arg = expression(p);
                    if (arg) nodelist_push(&args, arg);
                    while (match(p, T_NEWLINE));
                } while (match(p, T_COMMA));
            }
            consume(p, T_RBRACE, "Expected '}' after typed constructor arguments");

            AstNode *typed = ast_typed_init(expr->ident.name, args, line);
            ast_free(expr);
            expr = typed;
        } else if (match(p, T_LBRACKET)) {
            AstNode *idx = expression(p);
            consume(p, T_RBRACKET, "Expected ']' after index");
            if (expr && idx) {
                expr = ast_index(expr, idx, line);
            }
        } else if (match(p, T_DOT)) {
            if (!check(p, T_IDENT)) {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected field name after '.'",
                    "Use dot access like point.x");
                p->had_error = 1;
                return NULL;
            }
            const char *field = intern_string(token_str(&p->cur));
            advance(p);
            expr = ast_field(expr, field, line);
        } else if (match(p, T_INC)) {
            // Handle ++
            if (expr && expr->kind == NODE_IDENT) {
                AstNode *inc_n = ast_inc(expr->ident.name, line);
                ast_free(expr);
                expr = inc_n;
            } else {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "'++' can only be applied to variables",
                    "Use '++' only on variable names, e.g., 'count++'");
                p->had_error = 1;
                return NULL;
            }
        } else if (match(p, T_DEC)) {
            // Handle --
            if (expr && expr->kind == NODE_IDENT) {
                AstNode *dec_n = ast_dec(expr->ident.name, line);
                ast_free(expr);
                expr = dec_n;
            } else {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "'--' can only be applied to variables",
                    "Use '--' only on variable names, e.g., 'count--'");
                p->had_error = 1;
                return NULL;
            }
        } else {
            break;
        }
    }
    return expr;
}

static AstNode *unary(Parser *p) {
    int line = p->cur.line;
    if (match(p, T_NOT)) {
        AstNode *operand = unary(p);
        // Constant fold: !true → false, !false → true
        if (operand && operand->kind == NODE_BOOL)
            return ast_bool(!operand->boolean.value, line);
        return ast_not(operand, line);
    }

    if (match(p, T_MINUS)) {
        // Handle negative numbers: -x is equivalent to 0 - x
        AstNode *right = unary(p);
        if (right) {
            // Constant fold: -literal → literal with negated value
            if (right->kind == NODE_NUMBER) return ast_number(-right->number.value, line);
            if (right->kind == NODE_FLOAT) return ast_float(-right->fnumber.value, line);
            return ast_binop(OP_SUB, ast_number(0, line), right, line);
        }
    }
    if (match(p, T_PLUS)) {
        // Handle unary plus: +x is just x
        return unary(p);
    }
    return call_or_index(p);
}

// Constant Folding: if both children of a binop are numeric literals,
// evaluate at parse time and return a single literal node.
static AstNode *try_fold(BinOpKind op, AstNode *left, AstNode *right, int line) {
    if (!left || !right) return ast_binop(op, left, right, line);
    

    // Hava Nagile ... Hava Nagile ... Hava Nagile Ve'nishtanah!
    // Both int literals
    if (left->kind == NODE_NUMBER && right->kind == NODE_NUMBER) {
        long long l = left->number.value;
        long long r = right->number.value;
        switch (op) {
            case OP_ADD: return ast_number(l + r, line);
            case OP_SUB: return ast_number(l - r, line);
            case OP_MUL: return ast_number(l * r, line);
            case OP_DIV:
                if (r == 0) break; // don't fold div by zero
                if (l % r == 0) return ast_number(l / r, line);
                return ast_float((double)l / (double)r, line);
            case OP_MOD:
                if (r == 0) break;
                return ast_number(l % r, line);
            case OP_EQ:  return ast_bool(l == r, line);
            case OP_NEQ: return ast_bool(l != r, line);
            case OP_LT:  return ast_bool(l < r, line);
            case OP_GT:  return ast_bool(l > r, line);
            case OP_LTE: return ast_bool(l <= r, line);
            case OP_GTE: return ast_bool(l >= r, line);
            default: break;
        }
    }

    // Both float, or mixed int/float
    int l_num = (left->kind == NODE_NUMBER || left->kind == NODE_FLOAT);
    int r_num = (right->kind == NODE_NUMBER || right->kind == NODE_FLOAT);
    if (l_num && r_num && !(left->kind == NODE_NUMBER && right->kind == NODE_NUMBER)) {
        double dl = (left->kind == NODE_NUMBER) ? (double)left->number.value : left->fnumber.value;
        double dr = (right->kind == NODE_NUMBER) ? (double)right->number.value : right->fnumber.value;
        switch (op) {
            case OP_ADD: return ast_float(dl + dr, line);
            case OP_SUB: return ast_float(dl - dr, line);
            case OP_MUL: return ast_float(dl * dr, line);
            case OP_DIV:
                if (dr == 0.0) break;
                return ast_float(dl / dr, line);
            case OP_MOD: break; // skip float mod folding
            case OP_EQ:  return ast_bool(dl == dr, line);
            case OP_NEQ: return ast_bool(dl != dr, line);
            case OP_LT:  return ast_bool(dl < dr, line);
            case OP_GT:  return ast_bool(dl > dr, line);
            case OP_LTE: return ast_bool(dl <= dr, line);
            case OP_GTE: return ast_bool(dl >= dr, line);
            default: break;
        }
    }

    // Fold boolean literals for AND/OR
    if (left->kind == NODE_BOOL && right->kind == NODE_BOOL) {
        if (op == OP_AND) return ast_bool(left->boolean.value && right->boolean.value, line);
        if (op == OP_OR)  return ast_bool(left->boolean.value || right->boolean.value, line);
    }

    // Can't fold — return the binop as-is
    return ast_binop(op, left, right, line);
}

static AstNode *multiplication(Parser *p) {
    int line = p->cur.line;
    AstNode *expr = unary(p);
    while (check(p, T_MUL) || check(p, T_DIV) || check(p, T_MOD)) {
        if (p->had_error) break;
        TokenType op_type = p->cur.type;
        advance(p);
        AstNode *right = unary(p);
        BinOpKind kind = (op_type == T_MUL) ? OP_MUL : (op_type == T_DIV) ? OP_DIV : OP_MOD;
        if (expr && right) expr = try_fold(kind, expr, right, line);
    }
    return expr;
}

static AstNode *addition(Parser *p) {
    int line = p->cur.line;
    AstNode *expr = multiplication(p);
    while (check(p, T_PLUS) || check(p, T_MINUS)) {
        if (p->had_error) break;
        TokenType op_type = p->cur.type;
        advance(p);
        AstNode *right = multiplication(p);
        BinOpKind kind = (op_type == T_PLUS) ? OP_ADD : OP_SUB;
        if (expr && right) expr = try_fold(kind, expr, right, line);
    }
    return expr;
}

static AstNode *comparison(Parser *p) {
    int line = p->cur.line;
    AstNode *expr = addition(p);
    while (check(p, T_LT) || check(p, T_GT) || check(p, T_LTE) || check(p, T_GTE)) {
        if (p->had_error) break;
        TokenType op = p->cur.type;
        advance(p);
        AstNode *right = addition(p);
        BinOpKind kind = (op == T_LT) ? OP_LT : (op == T_GT) ? OP_GT : (op == T_LTE) ? OP_LTE : OP_GTE;
        if (expr && right) expr = try_fold(kind, expr, right, line);
    }
    return expr;
}

static AstNode *equality(Parser *p) {
    int line = p->cur.line;
    AstNode *expr = comparison(p);
    while (check(p, T_EQEQ) || check(p, T_NEQ)) {
        if (p->had_error) break;
        TokenType op = p->cur.type;
        advance(p);
        AstNode *right = comparison(p);
        BinOpKind kind = (op == T_EQEQ) ? OP_EQ : OP_NEQ;
        if (expr && right) expr = try_fold(kind, expr, right, line);
    }
    return expr;
}
static AstNode *logical_and(Parser *p) {
    int line = p->cur.line;
    AstNode *expr = equality(p);

    while (match(p, T_AND)) {
        AstNode *right = equality(p);
        if (expr && right) {
            expr = try_fold(OP_AND, expr, right, line);
        }
    }
    return expr;
}

static AstNode *logical_or(Parser *p) {
    int line = p->cur.line;
    AstNode *expr = logical_and(p);

    while (match(p, T_OR)) {
        AstNode *right = logical_and(p);
        if (expr && right) {
            expr = try_fold(OP_OR, expr, right, line);
        }
    }
    return expr;
}

static AstNode *expression(Parser *p) {
    if (p->had_error) return NULL;
    // Changed (was equality)
    return logical_or(p);
}

static AstNode *statement(Parser *p) {
    if (p->had_error) return NULL; // Stop parsing statements if error
    int line = p->cur.line;

    if (match(p, T_FUNC)) {
        return function_def(p);
    }

    if (match(p, T_DATA)) {
        return data_def(p, line, 0);
    }

    if (match(p, T_TEMPLATE)) {
        return data_def(p, line, 1);
    }

    if (match(p, T_BLOC)) {
        return bloc_def(p, line);
    }

    if (match(p, T_LET)) {
        return parse_declaration(p, 0);
    }

    if (match(p, T_CONST)) {
        return parse_declaration(p, 1);
    }

    if (match(p, T_EXPORT)) {
        AstNode *n = statement(p);
        if (!n) return NULL;
        if (n->kind != NODE_LET && n->kind != NODE_FUNC_DEF && n->kind != NODE_GROUP &&
            n->kind != NODE_DATA_DEF && n->kind != NODE_BLOC_DEF) {
            error_report_with_context(ERR_SYNTAX, line, 0,
                "export can only be used with let, const, func, data, or bloc declarations",
                "Use export let, export func, export data, or export bloc at module top level");
            p->had_error = 1;
            return NULL;
        }
        mark_stmt_exported(n);
        return n;
    }

    if (match(p, T_USE)) {
        if (check(p, T_STRING)) {
            AstNode *n = ast_import(token_str(&p->cur), NULL, 0, 1, line);
            free_token(&p->cur);
            advance(p);
            return n;
        }

        consume(p, T_LBRACE, "Expected '{' or string path after use");
        const char **names = NULL;
        int name_count = 0;
        while (!check(p, T_RBRACE) && !p->had_error) {
            if (!check(p, T_IDENT)) {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected exported name inside use list",
                    "Use syntax like use {greet, answer} from \"module.lu\"");
                p->had_error = 1;
                break;
            }
            names = realloc(names, sizeof(const char *) * (name_count + 1));
            names[name_count++] = intern_string(token_str(&p->cur));
            advance(p);
            if (!match(p, T_COMMA)) break;
        }
        consume(p, T_RBRACE, "Expected '}' after use list");
        consume(p, T_FROM, "Expected 'from' after use list");
        if (!check(p, T_STRING)) {
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                "Expected string path after from",
                "Use syntax like use {greet} from \"module.lu\"");
            p->had_error = 1;
            free(names);
            return NULL;
        }
        AstNode *n = ast_import(token_str(&p->cur), names, name_count, 1, line);
        free(names);
        free_token(&p->cur);
        advance(p);
        return n;
    }

    if (match(p, T_IMPORT)) {
        if (!check(p, T_STRING)) {
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                "Expected string path after import",
                "Use import \"path/to/file.lu\"");
            p->had_error = 1;
            return NULL;
        }
        AstNode *n = ast_import(token_str(&p->cur), NULL, 0, 0, line);
        free_token(&p->cur);
        advance(p);
        return n;
    }

    if (match(p, T_UNSAFE)) {
        match(p, T_NEWLINE);
        NodeList body;
        nodelist_init(&body);
        consume(p, T_LBRACE, "Expected '{' after unsafe");
        block(p, &body);
        if (!p->had_error) return ast_unsafe(body, line);
        return NULL;
    }

    if (match(p, T_PRINT)) {
        consume(p, T_LPAREN, "Expected '(' after print");
        NodeList args;
        nodelist_init(&args);
        if (!check(p, T_RPAREN)) {
            do {
                AstNode *arg = expression(p);
                if (arg) nodelist_push(&args, arg);
            } while (match(p, T_COMMA));
        }
        consume(p, T_RPAREN, "Expected ')' after print args");
        return ast_print(args, line);
    }
    if (match(p, T_RETURN)) {
        AstNode *expr = check(p, T_RBRACE) ? NULL : expression(p);
        return ast_return(expr, line);
    }
    if (match(p, T_BREAK)) {
        return ast_break(line);
    }
    if (match(p, T_CONTINUE)) {
        return ast_continue(line);
    }

    if (match(p, T_IF)) {
        consume(p, T_LPAREN, "Expected '(' after if");
        AstNode *cond = expression(p);
        consume(p, T_RPAREN, "Expected ')' after condition");

        // Allow newline between ')' and '{'
        match(p, T_NEWLINE);

        NodeList then_b;
        nodelist_init(&then_b);
        consume(p, T_LBRACE, "Expected '{'");
        block(p, &then_b);

        NodeList else_b;
        nodelist_init(&else_b);

        //Allow newline between '}' and 'else'
        match(p, T_NEWLINE);

        if (match(p, T_ELSE)) {
            // Allow newline after 'else' (before '{' or 'if')
            match(p, T_NEWLINE);

            if (check(p, T_IF)) {
                // Properly handle 'else if' by parsing it as a statement
                AstNode *elif = statement(p);
                if (elif) nodelist_push(&else_b, elif);
            } else {
                consume(p, T_LBRACE, "Expected '{'");
                block(p, &else_b);
            }
        }
        if (cond && !p->had_error) return ast_if(cond, then_b, else_b, line);
        return NULL;
    }

    if (match(p, T_WHILE)) {
        consume(p, T_LPAREN, "Expected '(' after while");
        AstNode *cond = expression(p);
        consume(p, T_RPAREN, "Expected ')'");

        // Allow newline before '{' in while
        match(p, T_NEWLINE);

        NodeList body;
        nodelist_init(&body);
        consume(p, T_LBRACE, "Expected '{'");
        block(p, &body);
        if (cond && !p->had_error) return ast_while(cond, body, line);
        return NULL;
    }

    if (match(p, T_FOR)) {
        consume(p, T_LPAREN, "Expected '(' after for");

        if (check(p, T_LET)) {
            advance(p);
            if (!check(p, T_IDENT)) {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected loop variable name in for-in loop",
                    "Use for (let item in list) { ... }");
                p->had_error = 1;
                return NULL;
            }
            const char *name = intern_string(token_str(&p->cur));
            advance(p);
            if (match(p, T_IN)) {
                AstNode *iterable = expression(p);
                consume(p, T_RPAREN, "Expected ')' after for-in iterable");
                match(p, T_NEWLINE);
                NodeList body;
                nodelist_init(&body);
                consume(p, T_LBRACE, "Expected '{' for loop body");
                block(p, &body);
                if (!p->had_error) return ast_for_in(name, iterable, body, line);
                return NULL;
            }

            AstNode *init_value = NULL;
            if (match(p, T_EQ)) {
                init_value = expression(p);
            }
            AstNode *init = ast_let(name, init_value, 0, line);
            consume(p, T_SEMICOLON, "Expected ';' after loop initializer");
            AstNode *cond = expression(p);
            consume(p, T_SEMICOLON, "Expected ';' after loop condition");
            AstNode *incr = statement(p);
            consume(p, T_RPAREN, "Expected ')' after loop increment");

            match(p, T_NEWLINE);
            NodeList body;
            nodelist_init(&body);
            consume(p, T_LBRACE, "Expected '{' for loop body");
            block(p, &body);
            if (!p->had_error) return ast_for(init, cond, incr, body, line);
            return NULL;
        }

        AstNode *init = statement(p);
        consume(p, T_SEMICOLON, "Expected ';' after loop initializer");
        AstNode *cond = expression(p);
        consume(p, T_SEMICOLON, "Expected ';' after loop condition");
        AstNode *incr = statement(p);
        consume(p, T_RPAREN, "Expected ')' after loop increment");

        // Allow newline before '{' in for
        match(p, T_NEWLINE);

        NodeList body;
        nodelist_init(&body);
        consume(p, T_LBRACE, "Expected '{' for loop body");
        block(p, &body);

        AstNode *n = NULL;
        // Only return if no errors occurred
        if (!p->had_error) n = ast_for(init, cond, incr, body, line);
        return n;
    }

    if (match(p, T_SWITCH)) {
        consume(p, T_LPAREN, "Expected '(' after switch");
        AstNode *expr = expression(p);
        consume(p, T_RPAREN, "Expected ')'");
        consume(p, T_LBRACE, "Expected '{' starting switch block");

        NodeList cases;
        nodelist_init(&cases);
        NodeList def_case;
        nodelist_init(&def_case);

        while (!check(p, T_RBRACE) && !check(p, T_EOF)) {
            if (p->had_error) break;
            int case_line = p->cur.line;
            
            if (match(p, T_CASE)) {
                AstNode *val = expression(p);
                consume(p, T_COLON, "Expected ':' after case value");
                NodeList cbody;
                nodelist_init(&cbody);

                while (!check(p, T_CASE) && !check(p, T_DEFAULT) && !check(p, T_RBRACE)) {
                    if (match(p, T_NEWLINE)) continue;
                    AstNode *stmt = statement(p);
                    if (stmt) nodelist_push(&cbody, stmt);
                }
                if (val) nodelist_push(&cases, ast_case(val, cbody, case_line));
            } else if (match(p, T_DEFAULT)) {
                consume(p, T_COLON, "Expected ':' after default");
                while (!check(p, T_CASE) && !check(p, T_DEFAULT) && !check(p, T_RBRACE)) {
                    if (match(p, T_NEWLINE)) continue;
                    AstNode *stmt = statement(p);
                    if (stmt) nodelist_push(&def_case, stmt);
                }
            } else {
                if (match(p, T_NEWLINE)) continue;
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected 'case' or 'default' inside switch",
                    "Switch blocks must contain 'case value:' or 'default:' statements");
                p->had_error = 1;
                return NULL;
            }
        }
        consume(p, T_RBRACE, "Expected '}' ending switch");
        if (expr && !p->had_error) return ast_switch(expr, cases, def_case, line);
        return NULL;
    }

    AstNode *expr = expression(p);
    if (match(p, T_EQ)) {
        // Case 1: Variable Assignment (x = 5)
        if (expr && expr->kind == NODE_IDENT) {
            AstNode *val = expression(p);
            AstNode *n = NULL;
            if (val && !p->had_error) n = ast_assign(expr->ident.name, val, line);
            ast_free(expr); // Free after creating assign node to preserve interned name pointer
            return n;
        }
        else if (expr && expr->kind == NODE_INDEX) {
            AstNode *target = expr->index.target;
            AstNode *index = expr->index.index;
            
            expr->index.target = NULL; 
            expr->index.index = NULL;
            ast_free(expr); 
            
            AstNode *val = expression(p);
            AstNode *n = NULL;
            if (val && !p->had_error) {
                n = ast_assign_index(target, index, val, line);
            } else {
                ast_free(target);
                ast_free(index);
            }
            return n;
        }
        else if (expr && expr->kind == NODE_FIELD) {
            AstNode *target = expr->field.target;
            AstNode *index = ast_string(expr->field.field, line);

            expr->field.target = NULL;
            ast_free(expr);

            AstNode *val = expression(p);
            AstNode *n = NULL;
            if (val && !p->had_error) {
                n = ast_assign_index(target, index, val, line);
            } else {
                ast_free(target);
                ast_free(index);
            }
            return n;
        } 
        else {
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                "Invalid assignment target",
                "Assign to variables, indices, or fields like x = 5, arr[0] = 5, obj.name = 5");
            p->had_error = 1;
            if(expr) ast_free(expr);
            return NULL;
        }
    }
    if (check(p, T_PLUS_EQ) || check(p, T_MINUS_EQ) || check(p, T_MUL_EQ) || check(p, T_DIV_EQ)) {
        TokenType compound = p->cur.type;
        advance(p);
        AstNode *rhs = expression(p);
        BinOpKind op = (compound == T_PLUS_EQ) ? OP_ADD :
                       (compound == T_MINUS_EQ) ? OP_SUB :
                       (compound == T_MUL_EQ) ? OP_MUL : OP_DIV;
        // Intresting...what does this do? Let's find out together :D
        // /// yea no fucking clue, just copied from assignment handling and modified a bit, hope it works :D
        if (expr && expr->kind == NODE_IDENT) {
            return ast_assign(expr->ident.name, ast_binop(op, ast_ident(expr->ident.name, line), rhs, line), line);
        }
        if (expr && expr->kind == NODE_INDEX) {
            AstNode *target = expr->index.target;
            AstNode *index = expr->index.index;
            expr->index.target = NULL;
            expr->index.index = NULL;
            AstNode *current = ast_index(clone_lvalue(target), clone_lvalue(index), line);
            AstNode *value = ast_binop(op, current, rhs, line);
            return ast_assign_index(target, index, value, line);
        }
        if (expr && expr->kind == NODE_FIELD) {
            AstNode *target = expr->field.target;
            AstNode *index = ast_string(expr->field.field, line);
            expr->field.target = NULL;
            AstNode *current = ast_field(clone_lvalue(target), expr->field.field, line);
            AstNode *value = ast_binop(op, current, rhs, line);
            ast_free(expr);
            return ast_assign_index(target, index, value, line);
        }

        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
            "Invalid compound assignment target",
            "Use +=, -=, *=, /= with variables, indices, or fields");
        p->had_error = 1;
        return NULL;
    }
    return expr;
}

static void block(Parser *p, NodeList *list) {
    while (!check(p, T_RBRACE) && !check(p, T_EOF)) {
        if (p->had_error) return; 
        if (match(p, T_NEWLINE)) continue;
        
        AstNode *stmt = statement(p);
        if (stmt) nodelist_push(list, stmt);
    }
    consume(p, T_RBRACE, "Expected '}'");
}

static AstNode *function_def(Parser *p) {
    if (p->had_error) return NULL;
    int line = p->cur.line;

    if (!check(p, T_IDENT)) {
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
            "Expected function name after 'func'",
            "Use 'func functionName(params) { ... }' to define a function");
        p->had_error = 1;
        return NULL;
    }
    const char *name = intern_string(token_str(&p->cur));
    advance(p);

    return function_literal(p, name, line);
}

static AstNode *data_def(Parser *p, int line, int is_template) {
    if (!check(p, T_IDENT)) {
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
            is_template ? "Expected template name after 'template'" : "Expected data type name after 'data'",
            is_template ? "Use template Shape { field1, field2 }" : "Use data Shape { field1, field2 }");
        p->had_error = 1;
        return NULL;
    }
    const char *name = intern_string(token_str(&p->cur));
    advance(p);
    consume(p, T_LBRACE, "Expected '{' after data type name");
    const char **fields = NULL;
    int field_count = 0;
    while (!check(p, T_RBRACE) && !p->had_error) {
        while (match(p, T_NEWLINE));
        if (!check(p, T_IDENT)) {
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                "Expected field name in data declaration",
                "Use comma-separated identifiers inside data { ... }");
            p->had_error = 1;
            break;
        }
        fields = realloc(fields, sizeof(const char *) * (size_t)(field_count + 1));
        fields[field_count++] = intern_string(token_str(&p->cur));
        advance(p);
        while (match(p, T_NEWLINE));
        if (!match(p, T_COMMA)) break;
    }
    consume(p, T_RBRACE, "Expected '}' after data fields");
    if (p->had_error) {
        free(fields);
        return NULL;
    }
    AstNode *n = ast_data_def(name, fields, field_count, is_template, line);
    free(fields);
    return n;
}

static AstNode *bloc_def(Parser *p, int line) {
    if (!check(p, T_IDENT)) {
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
            "Expected bloc type name after 'bloc'",
            "Use bloc Vec2 { x, y }");
        p->had_error = 1;
        return NULL;
    }
    const char *name = intern_string(token_str(&p->cur));
    advance(p);
    consume(p, T_LBRACE, "Expected '{' after bloc type name");
    const char **fields = NULL;
    int field_count = 0;
    while (!check(p, T_RBRACE) && !p->had_error) {
        while (match(p, T_NEWLINE));
        if (!check(p, T_IDENT)) {
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                "Expected field name in bloc declaration",
                "Use comma-separated identifiers inside bloc { ... }");
            p->had_error = 1;
            break;
        }
        fields = realloc(fields, sizeof(const char *) * (size_t)(field_count + 1));
        fields[field_count++] = intern_string(token_str(&p->cur));
        advance(p);
        while (match(p, T_NEWLINE));
        if (!match(p, T_COMMA)) break;
    }
    consume(p, T_RBRACE, "Expected '}' after bloc fields");
    if (p->had_error) {
        free(fields);
        return NULL;
    }
    AstNode *n = ast_bloc_def(name, fields, field_count, line);
    free(fields);
    return n;
}

static AstNode *function_literal(Parser *p, const char *name, int line) {
    consume(p, T_LPAREN, "Expected '('");
    const char **params = NULL;
    AstNode **defaults = NULL;
    int count = 0;
    if (!check(p, T_RPAREN)) {
        do {
            if (!check(p, T_IDENT)) {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected parameter name",
                    "Function parameters must be valid identifiers, e.g., 'func add(a, b)'");
                p->had_error = 1;
                if(params) free(params); // Pointers managed by intern table
                return NULL;
            }
            params = realloc(params, sizeof(const char*) * (count + 1));
            defaults = realloc(defaults, sizeof(AstNode*) * (count + 1));
            params[count++] = intern_string(token_str(&p->cur));
            advance(p);
            defaults[count - 1] = match(p, T_EQ) ? expression(p) : NULL;
        } while (match(p, T_COMMA));
    }
    consume(p, T_RPAREN, "Expected ')'");

    NodeList body;
    nodelist_init(&body);
    consume(p, T_LBRACE, "Expected '{'");
    p->inside_function = 1;
    block(p, &body);
    p->inside_function = 0;

    AstNode *n = NULL;
    if (!p->had_error) n = ast_funcdef(name, params, defaults, count, body, line);
    
    // No need to free name or params[i], they are managed by the intern table
    free(params);
    free(defaults);
    return n;
}

AstNode *parser_parse_program(Parser *p) {
    NodeList items;
    nodelist_init(&items);
    int line = p->cur.line;
    while (!check(p, T_EOF)) {
        if (p->had_error) break;
        
        if (match(p, T_NEWLINE)) continue;
        
        AstNode *stmt = NULL;
        if (match(p, T_FUNC)) {
            stmt = function_def(p);
        } else {
            stmt = statement(p);
        }

        if (stmt) nodelist_push(&items, stmt);
    }

    // If there was an error, verify we don't return a partial broken tree
    if (p->had_error) {
        nodelist_free(&items);
        return NULL;
    }

    AstNode *program = ast_block(items, line);
    if (!lint_unsafe_stmt(p, program, 0, 0)) {
        ast_free(program);
        return NULL;
    }
    return program;
}
