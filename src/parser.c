// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "ast.h"
#include "token.h"
#include "mystr.h"
#include "luna_error.h"

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

static AstNode *primary(Parser *p) {
    if (p->had_error) return NULL; // Error check

    if (check(p, T_NUMBER)) {
        long long v = p->cur.number;
        free_token(&p->cur);
        advance(p);
        return ast_number(v);
    }
    if (check(p, T_FLOAT)) {
        double v = p->cur.fnumber;
        free_token(&p->cur);
        advance(p);
        return ast_float(v);
    }
    if (check(p, T_STRING)) {
        char *s = my_strdup(p->cur.lexeme);
        free_token(&p->cur);
        advance(p);
        AstNode *n = ast_string(s);
        free(s);
        return n;
    }
    if (check(p, T_CHAR)) {
        char c = p->cur.lexeme[0];
        free_token(&p->cur);
        advance(p);
        return ast_char(c);
    }
    if (check(p, T_TRUE)) {
        match(p, T_TRUE);
        return ast_bool(1);
    }
    if (check(p, T_FALSE)) {
        match(p, T_FALSE);
        return ast_bool(0);
    }
    if (check(p, T_IDENT)) {
        char *name = my_strdup(p->cur.lexeme);
        free_token(&p->cur);
        advance(p);
        AstNode *n = ast_ident(name);
        free(name);
        return n;
    }
    if (match(p, T_LPAREN)) {
        AstNode *expr = expression(p);
        consume(p, T_RPAREN, "Expected ')' after expression");
        return expr;
    }
    if (match(p, T_LBRACKET)) {
        NodeList items;
        nodelist_init(&items);
        if (!check(p, T_RBRACKET)) {
            do {
                AstNode *elem = expression(p);
                if (elem) nodelist_push(&items, elem);
            } while (match(p, T_COMMA));
        }
        consume(p, T_RBRACKET, "Expected ']' at end of list");
        return ast_list(items);
    }
    // Parse input(...) expression with optional prompt string
    if (match(p, T_INPUT)) {
        consume(p, T_LPAREN, "Expected '(' after input");
        char *prompt = NULL;
        if (!check(p, T_RPAREN)) {
            if (check(p, T_STRING)) {
                prompt = my_strdup(p->cur.lexeme);
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
        AstNode *n = ast_input(prompt);
        if (prompt) free(prompt);
        return n;
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

            if (expr && expr->kind == NODE_IDENT) {
                char *name = my_strdup(expr->ident.name);
                ast_free(expr);
                expr = ast_call(name, args);
                free(name);
            } else {
                // Handling Logic error in parser
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Function call requires a function name",
                    "Only identifiers (function names) can be called, e.g., 'myFunction()'");
                p->had_error = 1;
                return NULL;
            }
        } else if (match(p, T_LBRACKET)) {
            AstNode *idx = expression(p);
            consume(p, T_RBRACKET, "Expected ']' after index");
            if (expr && idx) {
                expr = ast_index(expr, idx);
            }
        } else if (match(p, T_INC)) {
            // Handle ++
            if (expr && expr->kind == NODE_IDENT) {
                char *name = my_strdup(expr->ident.name);
                ast_free(expr);
                expr = ast_inc(name);
                free(name);
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
                char *name = my_strdup(expr->ident.name);
                ast_free(expr);
                expr = ast_dec(name);
                free(name);
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
    if (match(p, T_MINUS)) {
        // Handle negative numbers: -x is equivalent to 0 - x
        AstNode *right = unary(p);
        if (right) {
            return ast_binop(OP_SUB, ast_number(0), right);
        }
    }
    if (match(p, T_PLUS)) {
        // Handle unary plus: +x is just x
        return unary(p);
    }
    return call_or_index(p);
}

static AstNode *multiplication(Parser *p) {
    AstNode *expr = unary(p);
    while (check(p, T_MUL) || check(p, T_DIV) || check(p, T_MOD)) {
        if (p->had_error) break;
        TokenType op_type = p->cur.type;
        advance(p);
        AstNode *right = unary(p);
        BinOpKind kind = (op_type == T_MUL) ? OP_MUL : (op_type == T_DIV) ? OP_DIV : OP_MOD;
        if (expr && right) expr = ast_binop(kind, expr, right);
    }
    return expr;
}

static AstNode *addition(Parser *p) {
    AstNode *expr = multiplication(p);
    while (check(p, T_PLUS) || check(p, T_MINUS)) {
        if (p->had_error) break;
        TokenType op_type = p->cur.type;
        advance(p);
        AstNode *right = multiplication(p);
        BinOpKind kind = (op_type == T_PLUS) ? OP_ADD : OP_SUB;
        if (expr && right) expr = ast_binop(kind, expr, right);
    }
    return expr;
}

static AstNode *comparison(Parser *p) {
    AstNode *expr = addition(p);
    while (check(p, T_LT) || check(p, T_GT) || check(p, T_LTE) || check(p, T_GTE)) {
        if (p->had_error) break;
        TokenType op = p->cur.type;
        advance(p);
        AstNode *right = addition(p);
        BinOpKind kind = (op == T_LT) ? OP_LT : (op == T_GT) ? OP_GT : (op == T_LTE) ? OP_LTE : OP_GTE;
        if (expr && right) expr = ast_binop(kind, expr, right);
    }
    return expr;
}

static AstNode *equality(Parser *p) {
    AstNode *expr = comparison(p);
    while (check(p, T_EQEQ) || check(p, T_NEQ)) {
        if (p->had_error) break;
        TokenType op = p->cur.type;
        advance(p);
        AstNode *right = comparison(p);
        BinOpKind kind = (op == T_EQEQ) ? OP_EQ : OP_NEQ;
        if (expr && right) expr = ast_binop(kind, expr, right);
    }
    return expr;
}

static AstNode *expression(Parser *p) {
    if (p->had_error) return NULL;
    return equality(p);
}

static AstNode *statement(Parser *p) {
    if (p->had_error) return NULL; // Stop parsing statements if error

    if (match(p, T_FUNC)) {
        return function_def(p);
    }

    if (match(p, T_LET)) {
        // 1. Collect all variable names
        char **names = NULL;
        int name_count = 0;

        do {
            if (!check(p, T_IDENT)) {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected variable name after 'let' or ','",
                    "Variables must be identifiers (e.g., let a, b, c)");
                p->had_error = 1;
                // Cleanup names if error
                for(int i=0; i<name_count; i++) free(names[i]);
                free(names);
                return NULL;
            }
            
            names = realloc(names, sizeof(char*) * (name_count + 1));
            names[name_count++] = my_strdup(p->cur.lexeme);
            advance(p);
            
        } while (match(p, T_COMMA));

        // 2. Check for optional assignment
        AstNode **values = NULL;
        int val_count = 0;

        if (match(p, T_EQ)) {
            do {
                AstNode *val = expression(p);
                values = realloc(values, sizeof(AstNode*) * (val_count + 1));
                values[val_count++] = val;
            } while (match(p, T_COMMA));
        }

        // 3. Validation: Counts must match (unless no values provided at all)
        if (val_count > 0 && val_count != name_count) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Variable count (%d) does not match value count (%d)", name_count, val_count);
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col, msg,
                "Ensure you provide a value for every variable declared, or none at all.");
            p->had_error = 1;
            
            // Cleanup
            for(int i=0; i<name_count; i++) free(names[i]);
            free(names);
            for(int i=0; i<val_count; i++) ast_free(values[i]);
            free(values);
            return NULL;
        }

        // 4. Generate the AST
        NodeList lets;
        nodelist_init(&lets);

        for (int i = 0; i < name_count; i++) {
            // Use the corresponding value, or NULL if simply declaring
            AstNode *val = (val_count > 0) ? values[i] : NULL;
            
            AstNode *node = ast_let(names[i], val);
            nodelist_push(&lets, node);
            
            free(names[i]); 
        }

        free(names);
        if (values) free(values); 

        // Return single node or GROUP
        if (lets.count == 1) {
            AstNode *single = lets.items[0];
            free(lets.items); // Free the list array only
            return single;
        } else {
            // This ensures variables are defined in the CURRENT scope
            return ast_group(lets);
        }
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
        return ast_print(args);
    }
    if (match(p, T_RETURN)) {
        AstNode *expr = check(p, T_RBRACE) ? NULL : expression(p);
        return ast_return(expr);
    }
    if (match(p, T_BREAK)) {
        return ast_break();
    }
    if (match(p, T_CONTINUE)) {
        return ast_continue();
    }

    if (match(p, T_IF)) {
        consume(p, T_LPAREN, "Expected '(' after if");
        AstNode *cond = expression(p);
        consume(p, T_RPAREN, "Expected ')' after condition");

        NodeList then_b;
        nodelist_init(&then_b);
        consume(p, T_LBRACE, "Expected '{'");
        block(p, &then_b);

        NodeList else_b;
        nodelist_init(&else_b);
        if (match(p, T_ELSE)) {
            if (match(p, T_IF)) {
                consume(p, T_LPAREN, "Expected '('");
                AstNode *econd = expression(p);
                consume(p, T_RPAREN, "Expected ')'");
                consume(p, T_LBRACE, "Expected '{'");
                NodeList e_then;
                nodelist_init(&e_then);
                block(p, &e_then);
                NodeList e_else;
                nodelist_init(&e_else);
                if (econd) nodelist_push(&else_b, ast_if(econd, e_then, e_else));
            } else {
                consume(p, T_LBRACE, "Expected '{'");
                block(p, &else_b);
            }
        }
        if (cond && !p->had_error) return ast_if(cond, then_b, else_b);
        return NULL;
    }

    if (match(p, T_WHILE)) {
        consume(p, T_LPAREN, "Expected '(' after while");
        AstNode *cond = expression(p);
        consume(p, T_RPAREN, "Expected ')'");
        NodeList body;
        nodelist_init(&body);
        consume(p, T_LBRACE, "Expected '{'");
        block(p, &body);
        if (cond && !p->had_error) return ast_while(cond, body);
        return NULL;
    }

    if (match(p, T_FOR)) {
        consume(p, T_LPAREN, "Expected '(' after for");
        
        // 1. Initializer (e.g., let i = 0)
        AstNode *init = statement(p);
        consume(p, T_SEMICOLON, "Expected ';' after loop initializer");

        // 2. Condition (e.g., i < 10)
        AstNode *cond = expression(p);
        consume(p, T_SEMICOLON, "Expected ';' after loop condition");

        // 3. Increment (e.g., i++)
        AstNode *incr = statement(p);
        consume(p, T_RPAREN, "Expected ')' after loop increment");

        // 4. Body
        NodeList body;
        nodelist_init(&body);
        consume(p, T_LBRACE, "Expected '{' for loop body");
        block(p, &body);

        AstNode *n = NULL;
        // Only return if no errors occurred
        if (!p->had_error) n = ast_for(init, cond, incr, body);
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
                if (val) nodelist_push(&cases, ast_case(val, cbody));
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
        if (expr && !p->had_error) return ast_switch(expr, cases, def_case);
        return NULL;
    }

    // Assignment Handling
    AstNode *expr = expression(p);
    if (match(p, T_EQ)) {
        // Case 1: Variable Assignment (x = 5)
        if (expr && expr->kind == NODE_IDENT) {
            char *name = my_strdup(expr->ident.name);
            ast_free(expr);
            AstNode *val = expression(p);
            AstNode *n = NULL;
            if (val && !p->had_error) n = ast_assign(name, val);
            free(name);
            return n;
        }
        // Case 2: List Index Assignment (x[0] = 5)
        else if (expr && expr->kind == NODE_INDEX) {
            AstNode *target = expr->index.target;
            AstNode *index = expr->index.index;
            
            expr->index.target = NULL; 
            expr->index.index = NULL;
            ast_free(expr); 
            
            AstNode *val = expression(p);
            AstNode *n = NULL;
            if (val && !p->had_error) {
                n = ast_assign_index(target, index, val);
            } else {
                ast_free(target);
                ast_free(index);
            }
            return n;
        } 
        else {
            error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                "Invalid assignment target",
                "You can only assign to variables (e.g., 'x = 5') or list indices (e.g., 'arr[0] = 5')");
            p->had_error = 1;
            if(expr) ast_free(expr);
            return NULL;
        }
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

    if (!check(p, T_IDENT)) {
        error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
            "Expected function name after 'func'",
            "Use 'func functionName(params) { ... }' to define a function");
        p->had_error = 1;
        return NULL;
    }
    char *name = my_strdup(p->cur.lexeme);
    advance(p);

    consume(p, T_LPAREN, "Expected '('");
    char **params = NULL;
    int count = 0;
    if (!check(p, T_RPAREN)) {
        do {
            if (!check(p, T_IDENT)) {
                error_report_with_context(ERR_SYNTAX, p->cur.line, p->cur.col,
                    "Expected parameter name",
                    "Function parameters must be valid identifiers, e.g., 'func add(a, b)'");
                p->had_error = 1;
                free(name);
                if(params) free(params); // Better than crashing
                return NULL;
            }
            params = realloc(params, sizeof(char*) * (count + 1));
            params[count++] = my_strdup(p->cur.lexeme);
            advance(p);
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
    if (!p->had_error) n = ast_funcdef(name, params, count, body);
    
    free(name);
    for (int i = 0; i < count; i++) free(params[i]);
    free(params);
    return n;
}

AstNode *parser_parse_program(Parser *p) {
    NodeList items;
    nodelist_init(&items);
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
    return ast_block(items);
}