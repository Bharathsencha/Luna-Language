// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdlib.h>
#include <string.h>
#include "luna_chunk.h"

void luna_chunk_init(LunaChunk *chunk) {
    chunk->code = NULL;
    chunk->code_len = 0;
    chunk->code_cap = 0;
    chunk->line_map = NULL;
    chunk->line_len = 0;
    chunk->line_cap = 0;
    chunk->constants = NULL;
    chunk->const_len = 0;
    chunk->const_cap = 0;
    chunk->reg_count = 0;
    chunk->param_count = 0;
    chunk->upvalue_count = 0;
    chunk->name = NULL;
    chunk->subchunks = NULL;
    chunk->subchunk_len = 0;
    chunk->subchunk_cap = 0;
}

void luna_chunk_free(LunaChunk *chunk) {
    if (chunk->code) free(chunk->code);
    if (chunk->line_map) free(chunk->line_map);
    
    // Free constants
    for (size_t i = 0; i < chunk->const_len; i++) {
        value_free(chunk->constants[i]);
    }
    if (chunk->constants) free(chunk->constants);

    // Free subchunks
    for (int i = 0; i < chunk->subchunk_len; i++) {
        luna_chunk_free(chunk->subchunks[i]);
        free(chunk->subchunks[i]);
    }
    if (chunk->subchunks) free(chunk->subchunks);

    luna_chunk_init(chunk);
}

void luna_chunk_write(LunaChunk *chunk, uint8_t byte, int line) {
    if (chunk->code_len >= chunk->code_cap) {
        chunk->code_cap = chunk->code_cap ? chunk->code_cap * 2 : 128;
        chunk->code = realloc(chunk->code, chunk->code_cap * sizeof(uint8_t));
    }
    chunk->code[chunk->code_len++] = byte;

    if (chunk->line_len >= chunk->line_cap) {
        chunk->line_cap = chunk->line_cap ? chunk->line_cap * 2 : 128;
        chunk->line_map = realloc(chunk->line_map, chunk->line_cap * sizeof(int));
    }
    chunk->line_map[chunk->line_len++] = line;
}

int luna_chunk_add_constant(LunaChunk *chunk, Value val) {
    // Check if duplicate constant exists to save space (mostly for strings/idents)
    for (size_t i = 0; i < chunk->const_len; i++) {
        Value existing = chunk->constants[i];
        if (existing.type == val.type) {
            if (existing.type == VAL_INT && existing.i == val.i) return (int)i;
            if (existing.type == VAL_FLOAT && existing.f == val.f) return (int)i;
            if (existing.type == VAL_STRING && strcmp(existing.string->chars, val.string->chars) == 0) {
                value_free(val); // free duplicate copy
                return (int)i;
            }
        }
    }

    if (chunk->const_len >= chunk->const_cap) {
        chunk->const_cap = chunk->const_cap ? chunk->const_cap * 2 : 16;
        chunk->constants = realloc(chunk->constants, chunk->const_cap * sizeof(Value));
    }
    chunk->constants[chunk->const_len] = val;
    return (int)(chunk->const_len++);
}

int luna_chunk_add_subchunk(LunaChunk *chunk, LunaChunk *sub) {
    if (chunk->subchunk_len >= chunk->subchunk_cap) {
        chunk->subchunk_cap = chunk->subchunk_cap ? chunk->subchunk_cap * 2 : 8;
        chunk->subchunks = realloc(chunk->subchunks, chunk->subchunk_cap * sizeof(LunaChunk*));
    }
    chunk->subchunks[chunk->subchunk_len] = sub;
    return chunk->subchunk_len++;
}
