// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include "value.h"
#include "env.h" // We need the definition of Env

// Entry point for the interpreter

Value interpret(AstNode *program, Env *env); 
Value luna_call_value(Env *caller_env, Value callee, int argc, Value *argv, int line);
void interpreter_gc_mark_runtime_roots(void *ctx);

#endif