// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_RUNTIME_H
#define LUNA_RUNTIME_H

#include "ast.h"
#include "env.h"

typedef struct {
    Env *global_env;
    int stdlib_enabled;
    int initialized;
} LunaRuntime;

int luna_runtime_init(LunaRuntime *runtime, int with_stdlib);
void luna_runtime_shutdown(LunaRuntime *runtime);

AstNode *luna_parse_source(const char *source, const char *filename);
int luna_run_source(LunaRuntime *runtime, const char *source, const char *filename, AstNode **out_program);

Env *luna_runtime_env(LunaRuntime *runtime);
Value *luna_runtime_get_global(LunaRuntime *runtime, const char *name);

#endif