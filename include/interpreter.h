// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include "value.h"
#include "env.h" // We need the definition of Env

// Entry point for the interpreter
// Note: env_create and env_register_stdlib are now in env.h and library.h
Value interpret(AstNode *program, Env *env); 

#endif