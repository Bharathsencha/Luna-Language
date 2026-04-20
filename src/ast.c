// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "mystr.h"
#include "arena.h"
#include "intern.h"

Arena *ast_arena = NULL;

static CallKind classify_call(AstNode *callee) {
    if (!callee || callee->kind != NODE_IDENT) return CALL_GENERIC;

    const char *name = callee->ident.name;
    if (!name) return CALL_GENERIC;

    if (name == intern_string("alloc")) return CALL_ALLOC;
    if (name == intern_string("free")) return CALL_FREE;
    if (name == intern_string("deref") || name == intern_string("load")) return CALL_LOAD;
    if (name == intern_string("store")) return CALL_STORE;
    if (name == intern_string("ptr_add") || name == intern_string("ptr_offset")) return CALL_PTR_OFFSET;
    if (name == intern_string("addr") || name == intern_string("address_of")) return CALL_ADDRESS_OF;
    if (name == intern_string("defer")) return CALL_DEFER;
    if (name == intern_string("debug")) return CALL_DEBUG;
    if (name == intern_string("shape")) return CALL_SHAPE;
    if (name == intern_string("len")) return CALL_LEN;
    if (name == intern_string("append")) return CALL_APPEND;
    if (name == intern_string("type")) return CALL_TYPE;
    if (name == intern_string("int")) return CALL_INT;
    if (name == intern_string("float")) return CALL_FLOAT;
    return CALL_GENERIC;
}

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

AstNode *ast_template(const char **chunks, AstNode **exprs, int expr_count, int line) {
    AstNode *n = mk(NODE_TEMPLATE, line);
    n->template_string.expr_count = expr_count;
    n->template_string.chunks = arena_alloc(ast_arena, sizeof(const char *) * (expr_count + 1));
    n->template_string.exprs = arena_alloc(ast_arena, sizeof(AstNode *) * expr_count);
    for (int i = 0; i < expr_count + 1; i++) {
        n->template_string.chunks[i] = arena_strdup(ast_arena, chunks[i] ? chunks[i] : "");
    }
    for (int i = 0; i < expr_count; i++) {
        n->template_string.exprs[i] = exprs[i];
    }
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

AstNode *ast_map(const char **keys, AstNode **values, int count, int line) {
    AstNode *n = mk(NODE_MAP, line);
    n->map.keys = arena_alloc(ast_arena, sizeof(const char*) * count);
    n->map.values = arena_alloc(ast_arena, sizeof(AstNode*) * count);
    n->map.count = count;
    for (int i = 0; i < count; i++) {
        n->map.keys[i] = intern_string(keys[i]);
        n->map.values[i] = values[i];
    }
    return n;
}

AstNode *ast_ident(const char *name, int line) {
    AstNode *n = mk(NODE_IDENT, line);
    n->ident.name = intern_string(name);
    return n;
}

AstNode *ast_field(AstNode *target, const char *field, int line) {
    AstNode *n = mk(NODE_FIELD, line);
    n->field.target = target;
    n->field.field = intern_string(field);
    return n;
}

AstNode *ast_typed_init(const char *name, NodeList args, int line) {
    AstNode *n = mk(NODE_TYPED_INIT, line);
    n->typed_init.name = intern_string(name);
    n->typed_init.args = args;
    return n;
}

AstNode *ast_box_alloc(AstNode *size, int line) {
    AstNode *n = mk(NODE_BOX_ALLOC, line);
    n->box_alloc.size = size;
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

AstNode *ast_let(const char *name, AstNode *expr, int is_const, int line) {
    AstNode *n = mk(NODE_LET, line);
    n->let.name = intern_string(name);
    n->let.expr = expr;
    n->let.is_const = is_const;
    n->let.is_export = 0;
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

AstNode *ast_call(AstNode *callee, NodeList args, int line) {
    AstNode *n = mk(NODE_CALL, line);
    n->call.callee = callee;
    n->call.args = args;
    n->call.kind = classify_call(callee);
    return n;
}

AstNode *ast_index(AstNode *target, AstNode *index, int line) {
    AstNode *n = mk(NODE_INDEX, line);
    n->index.target = target;
    n->index.index = index;
    return n;
}

AstNode *ast_import(const char *path, const char **names, int name_count, int is_module_use, int line) {
    AstNode *n = mk(NODE_IMPORT, line);
    n->import_stmt.path = arena_strdup(ast_arena, path);
    n->import_stmt.name_count = name_count;
    n->import_stmt.is_module_use = is_module_use;
    if (name_count > 0) {
        n->import_stmt.names = arena_alloc(ast_arena, sizeof(const char *) * name_count);
        for (int i = 0; i < name_count; i++) {
            n->import_stmt.names[i] = intern_string(names[i]);
        }
    }
    return n;
}

AstNode *ast_unsafe(NodeList body, int line) {
    AstNode *n = mk(NODE_UNSAFE, line);
    n->unsafe_block.body = body;
    return n;
}

AstNode *ast_funcdef(const char *name, const char **params, AstNode **defaults, int count, NodeList body, int line) {
    AstNode *n = mk(NODE_FUNC_DEF, line);
    n->funcdef.name = name ? intern_string(name) : NULL;
    n->funcdef.params = arena_alloc(ast_arena, sizeof(const char*) * count);
    n->funcdef.defaults = arena_alloc(ast_arena, sizeof(AstNode*) * count);
    for (int i = 0; i < count; i++) {
        n->funcdef.params[i] = intern_string(params[i]);
        n->funcdef.defaults[i] = defaults ? defaults[i] : NULL;
    }
    n->funcdef.param_count = count;
    n->funcdef.body = body;
    n->funcdef.is_export = 0;
    return n;
}

AstNode *ast_data_def(const char *name, const char **fields, int field_count, int is_template, int line) {
    AstNode *n = mk(NODE_DATA_DEF, line);
    n->data_def.name = intern_string(name);
    n->data_def.fields = arena_alloc(ast_arena, sizeof(const char *) * field_count);
    n->data_def.field_count = field_count;
    n->data_def.is_export = 0;
    n->data_def.is_template = is_template;
    for (int i = 0; i < field_count; i++) {
        n->data_def.fields[i] = intern_string(fields[i]);
    }
    return n;
}

AstNode *ast_bloc_def(const char *name, const char **fields, int field_count, int line) {
    AstNode *n = mk(NODE_BLOC_DEF, line);
    n->bloc_def.name = intern_string(name);
    n->bloc_def.fields = arena_alloc(ast_arena, sizeof(const char *) * field_count);
    n->bloc_def.field_count = field_count;
    n->bloc_def.is_export = 0;
    for (int i = 0; i < field_count; i++) {
        n->bloc_def.fields[i] = intern_string(fields[i]);
    }
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

AstNode *ast_for_in(const char *name, AstNode *iterable, NodeList body, int line) {
    AstNode *n = mk(NODE_FOR_IN, line);
    n->forin.name = intern_string(name);
    n->forin.iterable = iterable;
    n->forin.body = body;
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

const char *ast_node_kind_name(NodeKind kind) {
    switch (kind) {
        case NODE_NUMBER: return "NODE_NUMBER";
        case NODE_FLOAT: return "NODE_FLOAT";
        case NODE_STRING: return "NODE_STRING";
        case NODE_TEMPLATE: return "NODE_TEMPLATE";
        case NODE_CHAR: return "NODE_CHAR";
        case NODE_BOOL: return "NODE_BOOL";
        case NODE_LIST: return "NODE_LIST";
        case NODE_MAP: return "NODE_MAP";
        case NODE_IDENT: return "NODE_IDENT";
        case NODE_FIELD: return "NODE_FIELD";
        case NODE_TYPED_INIT: return "NODE_TYPED_INIT";
        case NODE_BOX_ALLOC: return "NODE_BOX_ALLOC";
        case NODE_BINOP: return "NODE_BINOP";
        case NODE_LET: return "NODE_LET";
        case NODE_ASSIGN: return "NODE_ASSIGN";
        case NODE_ASSIGN_INDEX: return "NODE_ASSIGN_INDEX";
        case NODE_PRINT: return "NODE_PRINT";
        case NODE_INPUT: return "NODE_INPUT";
        case NODE_INC: return "NODE_INC";
        case NODE_DEC: return "NODE_DEC";
        case NODE_NOT: return "NODE_NOT";
        case NODE_IF: return "NODE_IF";
        case NODE_WHILE: return "NODE_WHILE";
        case NODE_FOR: return "NODE_FOR";
        case NODE_FOR_IN: return "NODE_FOR_IN";
        case NODE_BREAK: return "NODE_BREAK";
        case NODE_CONTINUE: return "NODE_CONTINUE";
        case NODE_SWITCH: return "NODE_SWITCH";
        case NODE_CASE: return "NODE_CASE";
        case NODE_BLOCK: return "NODE_BLOCK";
        case NODE_GROUP: return "NODE_GROUP";
        case NODE_CALL: return "NODE_CALL";
        case NODE_INDEX: return "NODE_INDEX";
        case NODE_IMPORT: return "NODE_IMPORT";
        case NODE_UNSAFE: return "NODE_UNSAFE";
        case NODE_FUNC_DEF: return "NODE_FUNC_DEF";
        case NODE_DATA_DEF: return "NODE_DATA_DEF";
        case NODE_BLOC_DEF: return "NODE_BLOC_DEF";
        case NODE_RETURN: return "NODE_RETURN";
        default: return "NODE_UNKNOWN";
    }
}

const char *ast_binop_kind_name(BinOpKind kind) {
    switch (kind) {
        case OP_ADD: return "OP_ADD";
        case OP_SUB: return "OP_SUB";
        case OP_MUL: return "OP_MUL";
        case OP_DIV: return "OP_DIV";
        case OP_MOD: return "OP_MOD";
        case OP_EQ: return "OP_EQ";
        case OP_NEQ: return "OP_NEQ";
        case OP_LT: return "OP_LT";
        case OP_GT: return "OP_GT";
        case OP_LTE: return "OP_LTE";
        case OP_GTE: return "OP_GTE";
        case OP_AND: return "OP_AND";
        case OP_OR: return "OP_OR";
        default: return "OP_UNKNOWN";
    }
}

//AST Node Destructor
void ast_free(AstNode *n) {
    // No-op for individual nodes because they are owned by ast_arena!
    (void)n;
}

static void release_nodelist(NodeList *list) {
    if (!list || !list->items) return;
    for (int i = 0; i < list->count; i++) {
        ast_release(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_release(AstNode *n) {
    if (!n) return;

    switch (n->kind) {
        case NODE_LIST:
            release_nodelist(&n->list.items);
            break;
        case NODE_MAP:
            for (int i = 0; i < n->map.count; i++) {
                ast_release(n->map.values[i]);
            }
            break;
        case NODE_TEMPLATE:
            for (int i = 0; i < n->template_string.expr_count; i++) {
                ast_release(n->template_string.exprs[i]);
            }
            break;
        case NODE_BINOP:
            ast_release(n->binop.left);
            ast_release(n->binop.right);
            break;
        case NODE_FIELD:
            ast_release(n->field.target);
            break;
        case NODE_TYPED_INIT:
            release_nodelist(&n->typed_init.args);
            break;
        case NODE_BOX_ALLOC:
            ast_release(n->box_alloc.size);
            break;
        case NODE_LET:
            ast_release(n->let.expr);
            break;
        case NODE_ASSIGN:
            ast_release(n->assign.expr);
            break;
        case NODE_ASSIGN_INDEX:
            ast_release(n->assign_index.list);
            ast_release(n->assign_index.index);
            ast_release(n->assign_index.value);
            break;
        case NODE_PRINT:
            release_nodelist(&n->print.args);
            break;
        case NODE_IF:
            ast_release(n->ifstmt.cond);
            release_nodelist(&n->ifstmt.then_block);
            release_nodelist(&n->ifstmt.else_block);
            break;
        case NODE_WHILE:
            ast_release(n->whilestmt.cond);
            release_nodelist(&n->whilestmt.body);
            break;
        case NODE_FOR:
            ast_release(n->forstmt.init);
            ast_release(n->forstmt.cond);
            ast_release(n->forstmt.incr);
            release_nodelist(&n->forstmt.body);
            break;
        case NODE_FOR_IN:
            ast_release(n->forin.iterable);
            release_nodelist(&n->forin.body);
            break;
        case NODE_SWITCH:
            ast_release(n->switchstmt.expr);
            release_nodelist(&n->switchstmt.cases);
            release_nodelist(&n->switchstmt.default_case);
            break;
        case NODE_CASE:
            ast_release(n->casestmt.value);
            release_nodelist(&n->casestmt.body);
            break;
        case NODE_BLOCK:
        case NODE_GROUP:
            release_nodelist(&n->block.items);
            break;
        case NODE_CALL:
            ast_release(n->call.callee);
            release_nodelist(&n->call.args);
            break;
        case NODE_INDEX:
            ast_release(n->index.target);
            ast_release(n->index.index);
            break;
        case NODE_FUNC_DEF:
            for (int i = 0; i < n->funcdef.param_count; i++) {
                ast_release(n->funcdef.defaults ? n->funcdef.defaults[i] : NULL);
            }
            release_nodelist(&n->funcdef.body);
            break;
        case NODE_DATA_DEF:
        case NODE_BLOC_DEF:
            break;
        case NODE_UNSAFE:
            release_nodelist(&n->unsafe_block.body);
            break;
        case NODE_RETURN:
            ast_release(n->ret.expr);
            break;
        case NODE_NOT:
            ast_release(n->logic_not.expr);
            break;
        default:
            break;
    }
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
