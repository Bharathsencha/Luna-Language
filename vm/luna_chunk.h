// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_CHUNK_H
#define LUNA_CHUNK_H

#include <stdint.h>
#include <stddef.h>
#include "value.h"

typedef struct LunaChunk {
    uint8_t *code;
    size_t   code_len;
    size_t   code_cap;

    int     *line_map;
    size_t   line_len;
    size_t   line_cap;

    Value   *constants;
    size_t   const_len;
    size_t   const_cap;

    int      reg_count;      // max registers needed by this chunk's stack frame
    int      param_count;    // number of expected arguments
    int      upvalue_count;  // number of upvalues captured by this chunk
    const char *name;        // function name or "<main>"

    struct LunaChunk **subchunks;
    int      subchunk_len;
    int      subchunk_cap;
} LunaChunk;

void luna_chunk_init(LunaChunk *chunk);
void luna_chunk_free(LunaChunk *chunk);
void luna_chunk_write(LunaChunk *chunk, uint8_t byte, int line);
int  luna_chunk_add_constant(LunaChunk *chunk, Value val);
int  luna_chunk_add_subchunk(LunaChunk *chunk, LunaChunk *sub);

#endif // LUNA_CHUNK_H
