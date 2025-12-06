// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef AST_H
#define AST_H

#include "value.h"

// Enumeration of all possible AST node types
typedef enum {
    NODE_NUMBER,
    NODE_FLOAT,     
    NODE_STRING,
    NODE_CHAR,      
    NODE_BOOL,
    NODE_LIST,      
    NODE_IDENT,

    NODE_BINOP,
    NODE_LET,
    NODE_ASSIGN,
    NODE_ASSIGN_INDEX,    
    NODE_PRINT,
    NODE_INPUT,
    NODE_INC,       // ++
    NODE_DEC,       // -- 

    NODE_IF,
    NODE_WHILE,
    NODE_FOR,       
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_SWITCH,
    NODE_CASE,
    NODE_BLOCK,
    NODE_GROUP,
    NODE_CALL,
    NODE_INDEX,     
    NODE_FUNC_DEF,
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
    OP_GTE
} BinOpKind;

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
        struct { char *text; } string;
        struct { char value; } character;  
        struct { int value; } boolean;
        struct { NodeList items; } list;  
        struct { char *name; } ident;
        struct { char *name; } inc; 
        struct { char *name; } dec; // for NODE_DEC

        struct { BinOpKind op; AstNode *left; AstNode *right; } binop;
        struct { char *name; AstNode *expr; } let;
        struct { char *name; AstNode *expr; } assign;
        struct { AstNode *list; AstNode *index; AstNode *value; } assign_index; 
        struct { AstNode *target; AstNode *index; } index; 

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
            AstNode *expr;
            NodeList cases;
            NodeList default_case;
        } switchstmt;

        struct { AstNode *value; NodeList body; } casestmt;
        struct { NodeList items; } block;
        struct { char *name; NodeList args; } call;

        struct {
            char *name;
            char **params;
            int param_count;
            NodeList body;
        } funcdef;

        struct { AstNode *expr; } ret;
    };
};

// List Management
void nodelist_init(NodeList *l);
void nodelist_push(NodeList *l, AstNode *n);
void nodelist_free(NodeList *l);

// Node Constructors
AstNode *ast_number(long long v);
AstNode *ast_float(double v); 
AstNode *ast_string(const char *s);
AstNode *ast_char(char c);   
AstNode *ast_bool(int v);
AstNode *ast_list(NodeList items); 
AstNode *ast_ident(const char *name);
AstNode *ast_inc(const char *name);
AstNode *ast_dec(const char *name);
AstNode *ast_binop(BinOpKind op, AstNode *l, AstNode *r);
AstNode *ast_let(const char *name, AstNode *expr);
AstNode *ast_assign(const char *name, AstNode *expr); 
AstNode *ast_print(NodeList args);
AstNode *ast_input(const char *prompt);
AstNode *ast_if(AstNode *cond, NodeList then_block, NodeList else_block);
AstNode *ast_while(AstNode *cond, NodeList body);
AstNode *ast_for(AstNode *init, AstNode *cond, AstNode *incr, NodeList body); 
AstNode *ast_break(void);
AstNode *ast_continue(void);
AstNode *ast_switch(AstNode *expr, NodeList cases, NodeList default_case);
AstNode *ast_case(AstNode *value, NodeList body);
AstNode *ast_block(NodeList items);
AstNode *ast_group(NodeList items);
AstNode *ast_call(const char *name, NodeList args);
AstNode *ast_index(AstNode *target, AstNode *index); 
AstNode *ast_funcdef(const char *name, char **params, int count, NodeList body);
AstNode *ast_return(AstNode *expr);
AstNode *ast_assign_index(AstNode *list, AstNode *index, AstNode *value);

void ast_free(AstNode *node);

#endif