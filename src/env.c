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

// Structure to hold a variable name and its current value
typedef struct {
    const char *name;
    Value val;
    int occupied; // Flag for hash table occupancy
    int is_const;
} VarEntry;


// The Environment structure, now using a Hash Table for variables
struct Env {
    VarEntry vars[TABLE_SIZE]; 
    struct Env *parent; // Pointer to the enclosing scope
    uint64_t version;   // Unique ID to validate loop cache hits against function recursion
    uint64_t scope_id;
    int occupied_slots[TABLE_SIZE]; // side array: indices of occupied var slots
    int occupied_count;             // number of entries in occupied_slots[]
    int gc_rooted;
    struct Env *gc_prev;
    struct Env *gc_next;
    int gc_captured_root;
    struct Env *gc_captured_prev;
    struct Env *gc_captured_next;
};

static uint64_t next_env_version = 1;
static uint64_t next_scope_id = 1;
static Env *gc_active_envs = NULL;
static Env *gc_captured_env_roots = NULL;

// Env freelist: avoids malloc/free per scope (every function call, if, loop body)
#define ENV_POOL_MAX 32
static Env *env_freelist[ENV_POOL_MAX];
static int env_freelist_count = 0;

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

static VarEntry *env_find_entry(Env *e, const char *name) {
    unsigned int h_base = hash_name(name);
    Env *cur_env = e;
    while (cur_env) {
        unsigned int h = h_base;
        unsigned int start_index = h;

        while (cur_env->vars[h].occupied) {
            if (cur_env->vars[h].name == name) {
                return &cur_env->vars[h];
            }
            h = (h + 1) & (TABLE_SIZE - 1);
            if (h == start_index) break;
        }
        cur_env = cur_env->parent;
    }
    return NULL;
}

static void env_gc_link_active(Env *e) {
    if (!e || e->gc_rooted) return;
    e->gc_rooted = 1;
    e->gc_prev = NULL;
    e->gc_next = gc_active_envs;
    if (gc_active_envs) gc_active_envs->gc_prev = e;
    gc_active_envs = e;
}

static void env_gc_unlink_active(Env *e) {
    if (!e || !e->gc_rooted) return;
    if (e->gc_prev) e->gc_prev->gc_next = e->gc_next;
    else gc_active_envs = e->gc_next;
    if (e->gc_next) e->gc_next->gc_prev = e->gc_prev;
    e->gc_prev = NULL;
    e->gc_next = NULL;
    e->gc_rooted = 0;
}

static void env_gc_link_captured_root(Env *e) {
    if (!e || e->gc_captured_root) return;
    e->gc_captured_root = 1;
    e->gc_captured_prev = NULL;
    e->gc_captured_next = gc_captured_env_roots;
    if (gc_captured_env_roots) gc_captured_env_roots->gc_captured_prev = e;
    gc_captured_env_roots = e;
}

static void env_gc_unlink_captured_root(Env *e) {
    if (!e || !e->gc_captured_root) return;
    if (e->gc_captured_prev) e->gc_captured_prev->gc_captured_next = e->gc_captured_next;
    else gc_captured_env_roots = e->gc_captured_next;
    if (e->gc_captured_next) e->gc_captured_next->gc_captured_prev = e->gc_captured_prev;
    e->gc_captured_prev = NULL;
    e->gc_captured_next = NULL;
    e->gc_captured_root = 0;
}

static void env_def_impl(Env *e, const char *name, Value *val, int move, int is_const) {
    unsigned int h = hash_name(name);
    unsigned int start_index = h;

    while (e->vars[h].occupied) {
        if (e->vars[h].name == name) {
            if (e->vars[h].is_const) {
                error_report_with_context(ERR_NAME, 0, 0,
                    "Cannot redefine const variable in the same scope",
                    "Use a different name or remove the reassignment");
                if (move) val->type = VAL_NULL;
                return;
            }
            value_free(e->vars[h].val);
            e->vars[h].val = move ? *val : value_copy(*val);
            e->vars[h].is_const = is_const;
            if (move) val->type = VAL_NULL;
            return;
        }
        h = (h + 1) & (TABLE_SIZE - 1);
        if (h == start_index) {
            fprintf(stderr, "Runtime Error: Environment variable limit reached.\n");
            if (move) val->type = VAL_NULL;
            return;
        }
    }

    e->vars[h].name = name;
    e->vars[h].val = move ? *val : value_copy(*val);
    e->vars[h].occupied = 1;
    e->vars[h].is_const = is_const;
    e->occupied_slots[e->occupied_count++] = h;
    if (move) val->type = VAL_NULL;
}

// Creates a new environment scope, linking it to a parent scope
// Pulls from freelist if available, otherwise calloc
Env *env_create(Env *parent) {
    Env *e;
    if (env_freelist_count > 0) {
        e = env_freelist[--env_freelist_count];
        // Only reset what matters — vars are tracked by occupied_slots
        e->occupied_count = 0;
    } else {
        e = calloc(1, sizeof(Env));
        if (!e) return NULL;
    }
    e->parent = parent;
    e->version = next_env_version++;
    e->scope_id = next_scope_id++;
    e->gc_prev = NULL;
    e->gc_next = NULL;
    e->gc_captured_root = 0;
    e->gc_captured_prev = NULL;
    e->gc_captured_next = NULL;
    env_gc_link_active(e);
    return e;
}

Env *env_snapshot(Env *e) {
    if (!e) return NULL;
    Env *parent_copy = env_snapshot(e->parent);
    Env *copy = calloc(1, sizeof(Env));
    if (!copy) return NULL;
    copy->parent = parent_copy;
    copy->version = e->version;
    copy->gc_rooted = 0;
    copy->gc_prev = NULL;
    copy->gc_next = NULL;
    copy->gc_captured_root = 0;
    copy->gc_captured_prev = NULL;
    copy->gc_captured_next = NULL;
    for (int i = 0; i < e->occupied_count; i++) {
        int idx = e->occupied_slots[i];
        if (!e->vars[idx].occupied) continue;
        copy->vars[idx].occupied = 1;
        copy->vars[idx].name = e->vars[idx].name;
        copy->vars[idx].is_const = e->vars[idx].is_const;
        copy->vars[idx].val = value_copy(e->vars[idx].val);
        copy->occupied_slots[copy->occupied_count++] = idx;
    }
    env_gc_link_captured_root(copy);
    return copy;
}

uint64_t env_get_version(Env *e) {
    return e ? e->version : 0;
}

void env_reset_version(Env *e) {
    if (e) e->version = next_env_version++;
}

uint64_t env_scope_id(Env *e) {
    return e ? e->scope_id : 0;
}

int env_is_global(Env *e) {
    return e && e->parent == NULL;
}

Env *env_root(Env *e) {
    Env *cur = e;
    while (cur && cur->parent) cur = cur->parent;
    return cur;
}

// Frees the environment and the memory used by its variables
// Returns to freelist instead of free() if pool has room
void env_free(Env *e) {
    if (!e) {
        return;
    }
    env_gc_unlink_active(e);
    env_gc_unlink_captured_root(e);
    // Only visit slots we actually used, not all 512
    for (int i = 0; i < e->occupied_count; i++) {
        int idx = e->occupied_slots[i];
        if (e->vars[idx].occupied) {
            value_free(e->vars[idx].val);
            e->vars[idx].occupied = 0;
            e->vars[idx].name = NULL;
            e->vars[idx].is_const = 0;
        }
    }
    e->occupied_count = 0;
    value_box_release_scope(e->scope_id);
    // Return to pool instead of free
    if (env_freelist_count < ENV_POOL_MAX) {
        env_freelist[env_freelist_count++] = e;
    } else {
        free(e);
    }
}

void env_free_chain(Env *e) {
    if (!e) return;
    Env *parent = e->parent;
    e->parent = NULL;
    env_free(e);
    env_free_chain(parent);
}

// Optimization: Clear all local variables to reuse an environment block during loops.
// This keeps pointers stable for AST lexical caching, avoiding mallocs per iteration.
// Uses occupied_slots[] so we only touch the 2-3 vars the loop body actually defined.
void env_clear_locals(Env *e) {
    if (!e) return;
    for (int i = 0; i < e->occupied_count; i++) {
        int idx = e->occupied_slots[i];
        value_free(e->vars[idx].val);
        e->vars[idx].occupied = 0; 
        e->vars[idx].name = NULL;
        e->vars[idx].is_const = 0;
    }
    e->occupied_count = 0;
    value_box_release_scope(e->scope_id);
}

Env *env_create_global(void) {
    return env_create(NULL);
}

void env_free_global(Env *env) {
    // Free the global env's variables
    if (env) {
        env_gc_unlink_active(env);
        env_gc_unlink_captured_root(env);
        for (int i = 0; i < env->occupied_count; i++) {
            int idx = env->occupied_slots[i];
            if (env->vars[idx].occupied) {
                value_free(env->vars[idx].val);
            }
        }
        value_box_release_scope(env->scope_id);
        free(env);
    }
    // Drain the freelist on shutdown
    for (int i = 0; i < env_freelist_count; i++) {
        free(env_freelist[i]);
    }
    env_freelist_count = 0;
}

// Looks up a variable by name using the hash table, traversing up the scope chain
Value *env_get(Env *e, const char *name) {
    VarEntry *entry = env_find_entry(e, name);
    return entry ? &entry->val : NULL;
}

Value *env_get_local(Env *e, const char *name) {
    if (!e || !name) return NULL;
    unsigned int h = hash_name(name);
    unsigned int start_index = h;
    while (e->vars[h].occupied) {
        if (e->vars[h].name == name) return &e->vars[h].val;
        h = (h + 1) & (TABLE_SIZE - 1);
        if (h == start_index) break;
    }
    return NULL;
}

Value *env_get_text(Env *e, const char *name) {
    if (!e || !name) return NULL;

    for (Env *cur = e; cur; cur = cur->parent) {
        for (int i = 0; i < cur->occupied_count; i++) {
            int idx = cur->occupied_slots[i];
            if (!cur->vars[idx].occupied || !cur->vars[idx].name) continue;
            if (strcmp(cur->vars[idx].name, name) == 0) {
                return &cur->vars[idx].val;
            }
        }
    }

    return NULL;
}

// Defines a new variable in the current scope using the hash table
void env_def(Env *e, const char *name, Value val) {
    env_def_impl(e, name, &val, 0, 0);
}

// Move variant of env_def: takes ownership of val, skips copy+free
void env_def_move(Env *e, const char *name, Value *val) {
    env_def_impl(e, name, val, 1, 0);
}

void env_def_const(Env *e, const char *name, Value val) {
    env_def_impl(e, name, &val, 0, 1);
}

void env_def_const_move(Env *e, const char *name, Value *val) {
    env_def_impl(e, name, val, 1, 1);
}

// Updates an existing variable, traversing up the scope chain
void env_assign(Env *e, const char *name, Value val) {
    VarEntry *target = env_find_entry(e, name);
    if (target) {
        if (target->is_const) {
            error_report_with_context(ERR_NAME, 0, 0,
                "Cannot assign to const variable",
                "Use 'let' for mutable variables or stop reassigning this name");
            return;
        }
        value_free(target->val);
        target->val = value_copy(val);
        return;
    }

    const char *suggestion = suggest_for_undefined_var(name);
    char msg[256];
    snprintf(msg, sizeof(msg), "Variable '%s' is not defined", name);
    error_report_with_context(ERR_NAME, 0, 0, 
        msg, 
        suggestion ? suggestion : "Declare variables with 'let' before assigning to them");
}

// Move variant of env_assign: takes ownership of val, skips copy+free
void env_assign_move(Env *e, const char *name, Value *val) {
    VarEntry *target = env_find_entry(e, name);
    if (target) {
        if (target->is_const) {
            error_report_with_context(ERR_NAME, 0, 0,
                "Cannot assign to const variable",
                "Use 'let' for mutable variables or stop reassigning this name");
            return;
        }
        value_free(target->val);
        target->val = *val;
        val->type = VAL_NULL;
        return;
    }

    const char *suggestion = suggest_for_undefined_var(name);
    char msg[256];
    snprintf(msg, sizeof(msg), "Variable '%s' is not defined", name);
    error_report_with_context(ERR_NAME, 0, 0, 
        msg, 
        suggestion ? suggestion : "Declare variables with 'let' before assigning to them");
}

int env_has_local(Env *e, const char *name) {
    if (!e) return 0;
    unsigned int h = hash_name(name);
    unsigned int start_index = h;
    while (e->vars[h].occupied) {
        if (e->vars[h].name == name) return 1;
        h = (h + 1) & (TABLE_SIZE - 1);
        if (h == start_index) break;
    }
    return 0;
}

// Defines a function in the current scope
void env_def_func(Env *e, const char *name, AstNode *def) {
    Value f;
    f.type = VAL_FUNCTION;
    f.func = def;
    env_def_move(e, name, &f);
}

// Looks up a function definition
AstNode *env_get_func(Env *e, const char *name) {
    Value *v = env_get(e, name);
    if (v && v->type == VAL_FUNCTION) {
        return v->func;
    }
    if (v && v->type == VAL_CLOSURE && v->closure) {
        return v->closure->funcdef;
    }
    return NULL;
}

void env_gc_mark_chain(Env *e, void *ctx) {
    for (Env *cur = e; cur; cur = cur->parent) {
        for (int i = 0; i < cur->occupied_count; i++) {
            int idx = cur->occupied_slots[i];
            if (!cur->vars[idx].occupied) continue;
            value_gc_mark(&cur->vars[idx].val, ctx);
        }
    }
}

void env_gc_mark_active_roots(void *ctx) {
    for (Env *env = gc_active_envs; env; env = env->gc_next) {
        env_gc_mark_chain(env, ctx);
    }
    for (Env *env = gc_captured_env_roots; env; env = env->gc_captured_next) {
        env_gc_mark_chain(env, ctx);
    }
}