// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef ENV_H
#define ENV_H

#include <stdint.h>
#include "gc.h"
#include "value.h"
#include "ast.h"

// Opaque type for the Environment
typedef struct Env Env;

// Scope Management
Env *env_create(Env *parent);
void env_free(Env *e);
Env *env_snapshot(Env *e);
void env_free_chain(Env *e);
void env_clear_locals(Env *e);
uint64_t env_get_version(Env *e);
void env_reset_version(Env *e);  // bump version after TCO rebind
uint64_t env_scope_id(Env *e);
int env_is_global(Env *e);
Env *env_root(Env *e);

// Global wrapper helpers (maintained for compatibility)
Env *env_create_global(void);
void env_free_global(Env *e);

// Variable Management
Value *env_get(Env *e, const char *name);
Value *env_get_local(Env *e, const char *name);
Value *env_get_text(Env *e, const char *name);
void env_def(Env *e, const char *name, Value val);
void env_def_move(Env *e, const char *name, Value *val);   // move variant (takes ownership)
void env_def_const(Env *e, const char *name, Value val);
void env_def_const_move(Env *e, const char *name, Value *val);
void env_assign(Env *e, const char *name, Value val);
void env_assign_move(Env *e, const char *name, Value *val); // move variant (takes ownership)
int env_has_local(Env *e, const char *name);

// Function Definition Management
void env_def_func(Env *e, const char *name, AstNode *def);
AstNode *env_get_func(Env *e, const char *name);
void env_gc_mark_active_roots(void *ctx);
void env_gc_mark_chain(Env *e, void *ctx);

#endif