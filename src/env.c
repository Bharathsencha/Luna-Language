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
    const char *name;
    Value val;
    int occupied; // Flag for hash table occupancy
} VarEntry;

// Structure to hold a function name and its AST definition
typedef struct {
    const char *name;
    AstNode *funcdef;
} FuncEntry;

// The Environment structure, now using a Hash Table for variables
struct Env {
    VarEntry vars[TABLE_SIZE]; 
    FuncEntry funcs[MAX_FUNCS];
    int func_count;
    struct Env *parent; // Pointer to the enclosing scope
    uint64_t version;   // Unique ID to validate loop cache hits against function recursion
};

static uint64_t next_env_version = 1;

// O(1) Pointer-Identity Hash 
// Since all names are interned, their memory address is mathematically unique for that string
static unsigned int hash_name(const char *str) {
    // Cast the pointer to an integer type. 
    // We shift right by 3 or 4 bits because pointers are typically aligned,
    // so the lowest bits are often zero.
    unsigned long long ptr_val = (unsigned long long)str;
    unsigned int hash = (unsigned int)((ptr_val >> 4) ^ (ptr_val >> 12));
    return hash & (TABLE_SIZE - 1);
}

// Creates a new environment scope, linking it to a parent scope
Env *env_create(Env *parent) {
    Env *e = calloc(1, sizeof(Env)); // Using calloc to zero-initialize the table
    if (e) {
        e->parent = parent;
        e->version = next_env_version++;
    }
    return e;
}

uint64_t env_get_version(Env *e) {
    return e ? e->version : 0;
}

// Frees the environment and the memory used by its variables
void env_free(Env *e) {
    if (!e) {
        return;
    }
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (e->vars[i].occupied) {
            // e->vars[i].name is an interned pointer so it never needs to be freed by env
            value_free(e->vars[i].val);
        }
    }
    // Function names are also interned, no free needed here
    free(e);
}

// Optimization: Clear all local variables to reuse an environment block during loops.
// This keeps pointers stable for AST lexical caching, avoiding mallocs per iteration.
void env_clear_locals(Env *e) {
    if (!e) return;
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (e->vars[i].occupied) {
            value_free(e->vars[i].val);
            e->vars[i].occupied = 0; 
            e->vars[i].name = NULL;
        }
    }
}

Env *env_create_global(void) {
    return env_create(NULL);
}

void env_free_global(Env *env) {
    env_free(env);
}

// Looks up a variable by name using the hash table, traversing up the scope chain
Value *env_get(Env *e, const char *name) {
    unsigned int h_base = hash_name(name);
    Env *cur_env = e;
    while (cur_env) {
        unsigned int h = h_base;
        unsigned int start_index = h;

        while (cur_env->vars[h].occupied) {
            // O(1) IDENTIFIER RESOLUTION: Direct pointer comparison
            if (cur_env->vars[h].name == name) {
                return &cur_env->vars[h].val;
            }
            h = (h + 1) & (TABLE_SIZE - 1);
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
        // If the variable already exists in the current scope, overwrite it (O(1) pointer match)
        if (e->vars[h].name == name) {
            value_free(e->vars[h].val);
            e->vars[h].val = value_copy(val);
            return;
        }
        h = (h + 1) & (TABLE_SIZE - 1);
        if (h == start_index) {
            fprintf(stderr, "Runtime Error: Environment variable limit reached.\n");
            return;
        }
    }

    // Insert new entry (O(1) assignment since name is already interned)
    e->vars[h].name = name;
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
    char msg[256];
    snprintf(msg, sizeof(msg), "Variable '%s' is not defined", name);
    error_report_with_context(ERR_NAME, 0, 0, 
        msg, 
        suggestion ? suggestion : "Declare variables with 'let' before assigning to them");
}

// Defines a function in the current scope
void env_def_func(Env *e, const char *name, AstNode *def) {
    if (e->func_count < MAX_FUNCS) {
        e->funcs[e->func_count].name = name; // Already interned by parser
        e->funcs[e->func_count].funcdef = def;
        e->func_count++;
    }
}

// Looks up a function definition
AstNode *env_get_func(Env *e, const char *name) {
    Env *cur_env = e;
    while (cur_env) {
        for (int i = 0; i < cur_env->func_count; i++) {
            // O(1) pointer comparison for functions
            if (cur_env->funcs[i].name == name) {
                return cur_env->funcs[i].funcdef;
            }
        }
        cur_env = cur_env->parent;
    }
    return NULL;
}