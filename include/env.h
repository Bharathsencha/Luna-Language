// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef ENV_H
#define ENV_H

#include "value.h"
#include "ast.h"

// Opaque type for the Environment
typedef struct Env Env;

// Scope Management
Env *env_create(Env *parent);
void env_free(Env *e);

// Global wrapper helpers (maintained for compatibility)
Env *env_create_global(void);
void env_free_global(Env *e);

// Variable Management
Value *env_get(Env *e, const char *name);
void env_def(Env *e, const char *name, Value val);
void env_assign(Env *e, const char *name, Value val);

// Function Definition Management
void env_def_func(Env *e, const char *name, AstNode *def);
AstNode *env_get_func(Env *e, const char *name);

#endif