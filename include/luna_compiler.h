// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_COMPILER_H
#define LUNA_COMPILER_H

#include "ast.h"
#include "luna_chunk.h"

LunaChunk *luna_compile_program(AstNode *program_ast);

#endif // LUNA_COMPILER_H
