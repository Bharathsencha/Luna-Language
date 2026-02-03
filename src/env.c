// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 202 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env.h"
#include "mystr.h"
#include "luna_error.h"
#include "gc.h" // Added: Required for mark_value

#define MAX_VARS 256
#define MAX_FUNCS 64

// Structure to hold a variable name and its current value
typedef struct {
    char *name;
    Value val;
} VarEntry;

// Structure to hold a function name and its AST definition
typedef struct {
    char *name;
    AstNode *funcdef;
} FuncEntry;

// The Environment structure, representing a scope (e.g., global, function, block)
struct Env {
    VarEntry vars[MAX_VARS];
    int var_count;
    FuncEntry funcs[MAX_FUNCS];
    int func_count;
    struct Env *parent; // Pointer to the enclosing scope
};

// Creates a new environment scope, linking it to a parent scope
Env *env_create(Env *parent) {
    Env *e = malloc(sizeof(Env));
    e->var_count = 0;
    e->func_count = 0;
    e->parent = parent;
    for (int i = 0; i < MAX_VARS; i++) {
        e->vars[i].name = NULL;
    }
    for (int i = 0; i < MAX_FUNCS; i++) {
        e->funcs[i].name = NULL;
    }
    return e;
}

// Frees the environment and the memory used by its variables
void env_free(Env *e) {
    if (!e) {
        return;
    }
    for (int i = 0; i < e->var_count; i++) {
        free(e->vars[i].name);
        value_free(e->vars[i].val); // For Objects, this is now a no-op (GC handles them)
    }
    for (int i = 0; i < e->func_count; i++) {
        free(e->funcs[i].name);
    }
    free(e);
}

Env *env_create_global(void) {
    return env_create(NULL);
}

void env_free_global(Env *env) {
    env_free(env);
}

// Looks up a variable by name, traversing up the scope chain if necessary
Value *env_get(Env *e, const char *name) {
    while (e) {
        // Search backwards to find the most recently declared variable (shadowing)
        for (int i = e->var_count - 1; i >= 0; i--) {
            if (!strcmp(e->vars[i].name, name)) {
                return &e->vars[i].val;
            }
        }
        e = e->parent;
    }
    return NULL;
}

// Defines a new variable in the current scope
void env_def(Env *e, const char *name, Value val) {
    if (e->var_count < MAX_VARS) {
        e->vars[e->var_count].name = my_strdup(name);
        e->vars[e->var_count].val = value_copy(val);
        e->var_count++;
    }
}

// Updates an existing variable, traversing up the scope chain
void env_assign(Env *e, const char *name, Value val) {
    Env *cur = e;
    while (cur) {
        for (int i = cur->var_count - 1; i >= 0; i--) {
            if (!strcmp(cur->vars[i].name, name)) {
                value_free(cur->vars[i].val); // No-op for objects
                cur->vars[i].val = value_copy(val);
                return;
            }
        }
        cur = cur->parent;
    }
    const char *suggestion = suggest_for_undefined_var(name);
    error_report(ERR_NAME, 0, 0, 
        suggestion ? suggestion : "Variable is not defined", 
        "Declare variables with 'let' before assigning to them");
}

// Defines a function in the current scope
void env_def_func(Env *e, const char *name, AstNode *def) {
    if (e->func_count < MAX_FUNCS) {
        e->funcs[e->func_count].name = my_strdup(name);
        e->funcs[e->func_count].funcdef = def;
        e->func_count++;
    }
}

// Looks up a function definition
AstNode *env_get_func(Env *e, const char *name) {
    while (e) {
        for (int i = 0; i < e->func_count; i++) {
            if (!strcmp(e->funcs[i].name, name)) {
                return e->funcs[i].funcdef;
            }
        }
        e = e->parent;
    }
    return NULL;
}

// Garbage Collection Helper 
//Cabbage

// Marks all variables in this environment (and parents) as active
void env_mark(Env *e) {
    while (e) {
        for (int i = 0; i < e->var_count; i++) {
            mark_value(e->vars[i].val);
        }
        e = e->parent;
    }
}