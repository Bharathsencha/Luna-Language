// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef AST_H
#define AST_H

#include <stdint.h>
#include "value.h"

// Enumeration of all possible AST node types
typedef enum {
    NODE_NUMBER,
    NODE_FLOAT,     
    NODE_STRING,
    NODE_TEMPLATE,
    NODE_CHAR,      
    NODE_BOOL,
    NODE_LIST,      
    NODE_MAP,
    NODE_IDENT,
    NODE_FIELD,
    NODE_TYPED_INIT,
    NODE_BOX_ALLOC,

    NODE_BINOP,
    NODE_LET,
    NODE_ASSIGN,
    NODE_ASSIGN_INDEX,    
    NODE_PRINT,
    NODE_INPUT,
    NODE_INC,       // ++
    NODE_DEC,       // -- 
    NODE_NOT,       // ! (Unary NOT)
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,       
    NODE_FOR_IN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_SWITCH,
    NODE_CASE,
    NODE_BLOCK,
    NODE_GROUP,
    NODE_CALL,
    NODE_INDEX,     
    NODE_IMPORT,
    NODE_UNSAFE,
    NODE_FUNC_DEF,
    NODE_DATA_DEF,
    NODE_BLOC_DEF,
    NODE_RETURN
} NodeKind;

// Binary operators supported by the language
typedef enum {
    OP_ADD, 
    OP_SUB, 
    OP_MUL, 
    OP_DIV, 
    OP_MOD,
    OP_EQ, 
    OP_NEQ, 
    OP_LT, 
    OP_GT, 
    OP_LTE, 
    OP_GTE,
    OP_AND,
    OP_OR,
} BinOpKind;

typedef enum {
    CALL_GENERIC,
    CALL_ALLOC,
    CALL_FREE,
    CALL_LOAD,
    CALL_STORE,
    CALL_PTR_OFFSET,
    CALL_ADDRESS_OF,
    CALL_DEFER,
    CALL_DEBUG,
    CALL_SHAPE,
    CALL_LEN,
    CALL_APPEND,
    CALL_TYPE,
    CALL_INT,
    CALL_FLOAT,
} CallKind;

typedef struct AstNode AstNode;

// Dynamic array of AST nodes (used for blocks, args, lists)
typedef struct {
    AstNode **items;
    int count;
    int capacity;
} NodeList;

// The main AST Node structure using a union for specific data
struct AstNode {
    NodeKind kind;
    int line;

    union {
        struct { long long value; } number; // Changed to long long
        struct { double value; } fnumber; 
        struct { char *text; Value cached; } string;
        struct {
            const char **chunks;
            AstNode **exprs;
            int expr_count;
        } template_string;
        struct { char value; } character;  
        struct { int value; } boolean;
        struct { NodeList items; } list;  
        struct {
            const char **keys;
            AstNode **values;
            int count;
        } map;
        struct {
            AstNode *target;
            const char *field;
        } field;
        struct {
            const char *name;
            NodeList args;
        } typed_init;
        struct { AstNode *size; } box_alloc;
        // Fast Local Caches for Identifier Binding (O(0) Lookups inside loops)
        struct { 
            const char *name;
            Value *cached_val; // Points directly to Environment slot
            uint64_t cached_env_version;
        } ident;
        
        struct { 
            const char *name; 
            Value *cached_val;
            uint64_t cached_env_version;
        } inc;
        
        struct { 
            const char *name; 
            Value *cached_val;
            uint64_t cached_env_version;
        } dec; // for NODE_DEC

        struct { BinOpKind op; AstNode *left; AstNode *right; } binop;
        struct { const char *name; AstNode *expr; int is_const; int is_export; } let;
        struct { 
            const char *name; 
            AstNode *expr; 
            Value *cached_val;
            uint64_t cached_env_version;
        } assign;
        struct { AstNode *list; AstNode *index; AstNode *value; } assign_index; 
        struct { AstNode *target; AstNode *index; } index; 

        struct { AstNode *expr; } logic_not; 

        struct { NodeList args; } print;
        struct { char *prompt; } input;

        struct { 
            AstNode *cond; 
            NodeList then_block; 
            NodeList else_block;
        } ifstmt;

        struct { AstNode *cond; NodeList body; } whilestmt;
        
        // Changed forstmt for C-style loops
        struct { 
            AstNode *init;  // e.g. let i = 0
            AstNode *cond;  // e.g. i < n
            AstNode *incr;  // e.g. i++
            NodeList body; 
        } forstmt;

        struct {
            const char *name;
            AstNode *iterable;
            NodeList body;
        } forin;

        struct {
            AstNode *expr;
            NodeList cases;
            NodeList default_case;
        } switchstmt;

        struct { AstNode *value; NodeList body; } casestmt;
        struct { NodeList items; } block;
        struct { AstNode *callee; NodeList args; CallKind kind; } call;
        struct {
            char *path;
            const char **names;
            int name_count;
            int is_module_use;
        } import_stmt;
        struct { NodeList body; } unsafe_block;

        struct {
            const char *name;
            const char **params;
            AstNode **defaults;
            int param_count;
            NodeList body;
            int is_export;
        } funcdef;

        struct {
            const char *name;
            const char **fields;
            int field_count;
            int is_export;
            int is_template;
        } data_def;
        struct {
            const char *name;
            const char **fields;
            int field_count;
            int is_export;
        } bloc_def;

        struct { AstNode *expr; } ret;
    };
};

// List Management
void nodelist_init(NodeList *l);
void nodelist_push(NodeList *l, AstNode *n);
void nodelist_free(NodeList *l);

// Node Constructors
AstNode *ast_number(long long v, int line);
AstNode *ast_float(double v, int line); 
AstNode *ast_string(const char *s, int line);
AstNode *ast_template(const char **chunks, AstNode **exprs, int expr_count, int line);
AstNode *ast_char(char c, int line);   
AstNode *ast_bool(int v, int line);
AstNode *ast_list(NodeList items, int line); 
AstNode *ast_map(const char **keys, AstNode **values, int count, int line);
AstNode *ast_ident(const char *name, int line);
AstNode *ast_field(AstNode *target, const char *field, int line);
AstNode *ast_typed_init(const char *name, NodeList args, int line);
AstNode *ast_box_alloc(AstNode *size, int line);
AstNode *ast_inc(const char *name, int line);
AstNode *ast_dec(const char *name, int line);
AstNode *ast_binop(BinOpKind op, AstNode *l, AstNode *r, int line);
AstNode *ast_let(const char *name, AstNode *expr, int is_const, int line);
AstNode *ast_assign(const char *name, AstNode *expr, int line); 
AstNode *ast_print(NodeList args, int line);
AstNode *ast_input(const char *prompt, int line);
AstNode *ast_if(AstNode *cond, NodeList then_block, NodeList else_block, int line);
AstNode *ast_while(AstNode *cond, NodeList body, int line);
AstNode *ast_for(AstNode *init, AstNode *cond, AstNode *incr, NodeList body, int line); 
AstNode *ast_for_in(const char *name, AstNode *iterable, NodeList body, int line);
AstNode *ast_break(int line);
AstNode *ast_continue(int line);
AstNode *ast_switch(AstNode *expr, NodeList cases, NodeList default_case, int line);
AstNode *ast_case(AstNode *value, NodeList body, int line);
AstNode *ast_block(NodeList items, int line);
AstNode *ast_group(NodeList items, int line);
AstNode *ast_call(AstNode *callee, NodeList args, int line);
AstNode *ast_index(AstNode *target, AstNode *index, int line); 
AstNode *ast_import(const char *path, const char **names, int name_count, int is_module_use, int line);
AstNode *ast_unsafe(NodeList body, int line);
AstNode *ast_funcdef(const char *name, const char **params, AstNode **defaults, int count, NodeList body, int line);
AstNode *ast_data_def(const char *name, const char **fields, int field_count, int is_template, int line);
AstNode *ast_bloc_def(const char *name, const char **fields, int field_count, int line);
AstNode *ast_return(AstNode *expr, int line);
AstNode *ast_assign_index(AstNode *list, AstNode *index, AstNode *value, int line);
AstNode *ast_not(AstNode *expr, int line);

const char *ast_node_kind_name(NodeKind kind);
const char *ast_binop_kind_name(BinOpKind kind);

void ast_free(AstNode *node);
void ast_release(AstNode *node);
void ast_init(void);
void ast_cleanup(void);

#endif
