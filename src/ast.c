// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "mystr.h"

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
    AstNode *n = malloc(sizeof(AstNode));
    if (n) {
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
    n->string.text = my_strdup(s);
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
    n->ident.name = my_strdup(name);
    return n;
}

// Increment 
AstNode *ast_inc(const char *name, int line) {
    AstNode *n = mk(NODE_INC, line);
    n->inc.name = my_strdup(name);
    return n;
}

// Decrement
AstNode *ast_dec(const char *name, int line) {
    AstNode *n = mk(NODE_DEC, line);
    n->dec.name = my_strdup(name);
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
    n->let.name = my_strdup(name);
    n->let.expr = expr;
    return n;
}

AstNode *ast_assign(const char *name, AstNode *expr, int line) {
    AstNode *n = mk(NODE_ASSIGN, line);
    n->assign.name = my_strdup(name);
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
    n->input.prompt = prompt ? my_strdup(prompt) : NULL;
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
    n->call.name = my_strdup(name);
    n->call.args = args;
    return n;
}

AstNode *ast_index(AstNode *target, AstNode *index, int line) {
    AstNode *n = mk(NODE_INDEX, line);
    n->index.target = target;
    n->index.index = index;
    return n;
}

AstNode *ast_funcdef(const char *name, char **params, int count, NodeList body, int line) {
    AstNode *n = mk(NODE_FUNC_DEF, line);
    n->funcdef.name = my_strdup(name);
    n->funcdef.params = malloc(sizeof(char*) * count);
    for (int i = 0; i < count; i++) {
        n->funcdef.params[i] = my_strdup(params[i]);
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
    if (!n) {
        return;
    }
    switch (n->kind) {
        case NODE_STRING:
            free(n->string.text);
            break;
        case NODE_IDENT:
            free(n->ident.name);
            break;
        case NODE_INC:
            free(n->inc.name);
            break;
        case NODE_DEC:  
            free(n->dec.name);
            break;
        case NODE_LIST:
            nodelist_free(&n->list.items);
            break;
        case NODE_BINOP:
            ast_free(n->binop.left);
            ast_free(n->binop.right);
            break;
        case NODE_LET:
            free(n->let.name);
            ast_free(n->let.expr);
            break;
        case NODE_ASSIGN:
            free(n->assign.name);
            ast_free(n->assign.expr);
            break;
        case NODE_PRINT:
            nodelist_free(&n->print.args);
            break;
        case NODE_INPUT:
            if (n->input.prompt) {
                free(n->input.prompt);
            }
            break;
        case NODE_IF:
            ast_free(n->ifstmt.cond);
            nodelist_free(&n->ifstmt.then_block);
            nodelist_free(&n->ifstmt.else_block);
            break;
        case NODE_WHILE:
            ast_free(n->whilestmt.cond);
            nodelist_free(&n->whilestmt.body);
            break;
        case NODE_FOR:
            ast_free(n->forstmt.init);
            ast_free(n->forstmt.cond);
            ast_free(n->forstmt.incr);
            nodelist_free(&n->forstmt.body);
            break;
        case NODE_SWITCH:
            ast_free(n->switchstmt.expr);
            nodelist_free(&n->switchstmt.cases);
            nodelist_free(&n->switchstmt.default_case);
            break;
        case NODE_CASE:
            ast_free(n->casestmt.value);
            nodelist_free(&n->casestmt.body);
            break;
        case NODE_BLOCK:
            nodelist_free(&n->block.items);
            break;
        case NODE_CALL:
            free(n->call.name);
            nodelist_free(&n->call.args);
            break;
        case NODE_INDEX:
            ast_free(n->index.target);
            ast_free(n->index.index);
            break;
        case NODE_FUNC_DEF:
            free(n->funcdef.name);
            for (int i = 0; i < n->funcdef.param_count; i++) {
                free(n->funcdef.params[i]);
            }
            free(n->funcdef.params);
            nodelist_free(&n->funcdef.body);
            break;
        case NODE_RETURN:
            ast_free(n->ret.expr);
            break;
        case NODE_ASSIGN_INDEX:
            ast_free(n->assign_index.list);
            ast_free(n->assign_index.index);
            ast_free(n->assign_index.value);
            break;
        case NODE_NOT:
            ast_free(n->logic_not.expr);
            break;
        default:
            break;
    }
    free(n);
}