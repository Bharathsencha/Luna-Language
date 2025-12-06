// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include "value.h"

typedef struct Env Env;

//Helper functions to manage the global scope externally
Env *env_create_global(void);
void env_free_global(Env *env);

// Registers standard library functions (Math/String) into the environment
void env_register_stdlib(Env *env);

// Entry point for the interpreter
Value interpret(AstNode *program, Env *env); 

#endif