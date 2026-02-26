// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "mystr.h"
#include "arena.h"
#include "intern.h"

Arena *ast_arena = NULL;

//Node List Management

void nodelist_init(NodeList *l) {
    l->items = NULL;
    l->count = 0;
    l->capacity = 0;
}

void nodelist_push(NodeList *l, AstNode *n) {
    if (l->count >= l->capacity) {
        int nc = (l->capacity == 0) ? 4 : l->capacity * 2;
        l->items = realloc(l->items, sizeof(AstNode*) * nc);
        l->capacity = nc;
    }
    l->items[l->count++] = n;
}

void nodelist_free(NodeList *l) {
    if (!l) {
        return;
    }
    for (int i = 0; i < l->count; i++) {
        ast_free(l->items[i]);
    }
    free(l->items);
    l->items = NULL;
    l->count = 0;
}

//AST Node Constructors

static AstNode *mk(NodeKind k, int line) {
    if (!ast_arena) ast_init();
    AstNode *n = arena_alloc(ast_arena, sizeof(AstNode));
    if (n) {
        memset(n, 0, sizeof(AstNode));
        n->kind = k;
        n->line = line;
    }
    return n;
}

AstNode *ast_number(long long v, int line) {
    AstNode *n = mk(NODE_NUMBER, line);
    n->number.value = v;
    return n;
}

AstNode *ast_float(double v, int line) {
    AstNode *n = mk(NODE_FLOAT, line);
    n->fnumber.value = v;
    return n;
}

AstNode *ast_string(const char *s, int line) {
    AstNode *n = mk(NODE_STRING, line);
    n->string.text = arena_strdup(ast_arena, s);
    return n;
}

AstNode *ast_char(char c, int line) {
    AstNode *n = mk(NODE_CHAR, line);
    n->character.value = c;
    return n;
}

AstNode *ast_bool(int v, int line) {
    AstNode *n = mk(NODE_BOOL, line);
    n->boolean.value = !!v;
    return n;
}

AstNode *ast_list(NodeList items, int line) {
    AstNode *n = mk(NODE_LIST, line);
    n->list.items = items;
    return n;
}

AstNode *ast_ident(const char *name, int line) {
    AstNode *n = mk(NODE_IDENT, line);
    n->ident.name = intern_string(name);
    return n;
}

// Increment 
AstNode *ast_inc(const char *name, int line) {
    AstNode *n = mk(NODE_INC, line);
    n->inc.name = intern_string(name);
    return n;
}

// Decrement
AstNode *ast_dec(const char *name, int line) {
    AstNode *n = mk(NODE_DEC, line);
    n->dec.name = intern_string(name);
    return n;
}

AstNode *ast_binop(BinOpKind op, AstNode *l, AstNode *r, int line) {
    AstNode *n = mk(NODE_BINOP, line);
    n->binop.op = op;
    n->binop.left = l;
    n->binop.right = r;
    return n;
}

AstNode *ast_let(const char *name, AstNode *expr, int line) {
    AstNode *n = mk(NODE_LET, line);
    n->let.name = intern_string(name);
    n->let.expr = expr;
    return n;
}

AstNode *ast_assign(const char *name, AstNode *expr, int line) {
    AstNode *n = mk(NODE_ASSIGN, line);
    n->assign.name = intern_string(name);
    n->assign.expr = expr;
    return n;
}

AstNode *ast_print(NodeList args, int line) {
    AstNode *n = mk(NODE_PRINT, line);
    n->print.args = args;
    return n;
}

AstNode *ast_input(const char *prompt, int line) {
    AstNode *n = mk(NODE_INPUT, line);
    n->input.prompt = prompt ? arena_strdup(ast_arena, prompt) : NULL;
    return n;
}

AstNode *ast_if(AstNode *cond, NodeList then_b, NodeList else_b, int line) {
    AstNode *n = mk(NODE_IF, line);
    n->ifstmt.cond = cond;
    n->ifstmt.then_block = then_b;
    n->ifstmt.else_block = else_b;
    return n;
}

AstNode *ast_while(AstNode *cond, NodeList body, int line) {
    AstNode *n = mk(NODE_WHILE, line);
    n->whilestmt.cond = cond;
    n->whilestmt.body = body;
    return n;
}

AstNode *ast_break(int line) {
    return mk(NODE_BREAK, line);
}

AstNode *ast_continue(int line) {
    return mk(NODE_CONTINUE, line);
}

AstNode *ast_switch(AstNode *expr, NodeList cases, NodeList def, int line) {
    AstNode *n = mk(NODE_SWITCH, line);
    n->switchstmt.expr = expr;
    n->switchstmt.cases = cases;
    n->switchstmt.default_case = def;
    return n;
}

AstNode *ast_case(AstNode *value, NodeList body, int line) {
    AstNode *n = mk(NODE_CASE, line);
    n->casestmt.value = value;
    n->casestmt.body = body;
    return n;
}

AstNode *ast_block(NodeList items, int line) {
    AstNode *n = mk(NODE_BLOCK, line);
    n->block.items = items;
    return n;
}

AstNode *ast_group(NodeList items, int line) {
    AstNode *n = mk(NODE_GROUP, line);
    n->block.items = items; 
    return n;
}

AstNode *ast_call(const char *name, NodeList args, int line) {
    AstNode *n = mk(NODE_CALL, line);
    n->call.name = intern_string(name);
    n->call.args = args;
    return n;
}

AstNode *ast_index(AstNode *target, AstNode *index, int line) {
    AstNode *n = mk(NODE_INDEX, line);
    n->index.target = target;
    n->index.index = index;
    return n;
}

AstNode *ast_funcdef(const char *name, const char **params, int count, NodeList body, int line) {
    AstNode *n = mk(NODE_FUNC_DEF, line);
    n->funcdef.name = intern_string(name);
    n->funcdef.params = arena_alloc(ast_arena, sizeof(const char*) * count);
    for (int i = 0; i < count; i++) {
        n->funcdef.params[i] = intern_string(params[i]);
    }
    n->funcdef.param_count = count;
    n->funcdef.body = body;
    return n;
}

AstNode *ast_return(AstNode *expr, int line) {
    AstNode *n = mk(NODE_RETURN, line);
    n->ret.expr = expr;
    return n;
}

AstNode *ast_for(AstNode *init, AstNode *cond, AstNode *incr, NodeList body, int line) {
    AstNode *n = mk(NODE_FOR, line);
    n->forstmt.init = init;
    n->forstmt.cond = cond;
    n->forstmt.incr = incr;
    n->forstmt.body = body;
    return n;
}

AstNode *ast_assign_index(AstNode *list, AstNode *index, AstNode *value, int line) {
    AstNode *n = mk(NODE_ASSIGN_INDEX, line);
    n->assign_index.list = list;
    n->assign_index.index = index;
    n->assign_index.value = value;
    return n;
}

AstNode *ast_not(AstNode *expr, int line) {
    AstNode *n = mk(NODE_NOT, line);
    n->logic_not.expr = expr;
    return n;
}

//AST Node Destructor
void ast_free(AstNode *n) {
    // No-op for individual nodes because they are owned by ast_arena!
    (void)n;
}

void ast_init(void) {
    if (!ast_arena) {
        ast_arena = arena_create(1024 * 1024 * 4); // 4MB default
    }
}

void ast_cleanup(void) {
    if (ast_arena) {
        arena_free(ast_arena);
        ast_arena = NULL;
    }
}