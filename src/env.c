// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env.h"
#include "mystr.h"
#include "luna_error.h"

// Increased size and switched to power of 2 for better hash distribution
#define TABLE_SIZE 512
#define MAX_FUNCS 64

// Structure to hold a variable name and its current value
typedef struct {
    char *name;
    Value val;
    int occupied; // Flag for hash table occupancy
} VarEntry;

// Structure to hold a function name and its AST definition
typedef struct {
    char *name;
    AstNode *funcdef;
} FuncEntry;

// The Environment structure, now using a Hash Table for variables
struct Env {
    VarEntry vars[TABLE_SIZE]; 
    FuncEntry funcs[MAX_FUNCS];
    int func_count;
    struct Env *parent; // Pointer to the enclosing scope
};

// djb2 hash algorithm for fast, high-quality string hashing
static unsigned int hash_name(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % TABLE_SIZE;
}

// Creates a new environment scope, linking it to a parent scope
Env *env_create(Env *parent) {
    Env *e = calloc(1, sizeof(Env)); // Using calloc to zero-initialize the table
    if (e) {
        e->parent = parent;
    }
    return e;
}

// Frees the environment and the memory used by its variables
void env_free(Env *e) {
    if (!e) {
        return;
    }
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (e->vars[i].occupied) {
            free(e->vars[i].name);
            value_free(e->vars[i].val);
        }
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

// Looks up a variable by name using the hash table, traversing up the scope chain
Value *env_get(Env *e, const char *name) {
    Env *cur_env = e;
    while (cur_env) {
        unsigned int h = hash_name(name);
        unsigned int start_index = h;

        while (cur_env->vars[h].occupied) {
            if (strcmp(cur_env->vars[h].name, name) == 0) {
                return &cur_env->vars[h].val;
            }
            h = (h + 1) % TABLE_SIZE;
            if (h == start_index) break; // Table is full and item not found
        }
        cur_env = cur_env->parent;
    }
    return NULL;
}

// Defines a new variable in the current scope using the hash table
void env_def(Env *e, const char *name, Value val) {
    unsigned int h = hash_name(name);
    unsigned int start_index = h;

    while (e->vars[h].occupied) {
        // If the variable already exists in the current scope, overwrite it
        if (strcmp(e->vars[h].name, name) == 0) {
            value_free(e->vars[h].val);
            e->vars[h].val = value_copy(val);
            return;
        }
        h = (h + 1) % TABLE_SIZE;
        if (h == start_index) {
            fprintf(stderr, "Runtime Error: Environment variable limit reached.\n");
            return;
        }
    }

    // Insert new entry
    e->vars[h].name = my_strdup(name);
    e->vars[h].val = value_copy(val);
    e->vars[h].occupied = 1;
}

// Updates an existing variable, traversing up the scope chain
void env_assign(Env *e, const char *name, Value val) {
    Value *target = env_get(e, name);
    if (target) {
        value_free(*target);
        *target = value_copy(val);
        return;
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
    Env *cur_env = e;
    while (cur_env) {
        for (int i = 0; i < cur_env->func_count; i++) {
            if (strcmp(cur_env->funcs[i].name, name) == 0) {
                return cur_env->funcs[i].funcdef;
            }
        }
        cur_env = cur_env->parent;
    }
    return NULL;
}