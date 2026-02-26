// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "arena.h"

struct Arena {
    uint8_t *buffer;
    size_t size;
    size_t offset;
    struct Arena *next;
};

Arena *arena_create(size_t size) {
    Arena *arena = malloc(sizeof(Arena));
    if (!arena) return NULL;
    arena->buffer = malloc(size);
    arena->size = size;
    arena->offset = 0;
    arena->next = NULL;
    return arena;
}

void *arena_alloc(Arena *arena, size_t size) {
    // 8-byte alignment for safety
    size_t aligned_size = (size + 7) & ~7;
    
    Arena *current = arena;
    while (current->offset + aligned_size > current->size) {
        if (!current->next) {
            // Give the new block at least the original size to prevent tiny fragments
            size_t new_size = current->size > aligned_size ? current->size : aligned_size;
            // Scale up for efficiency
            new_size *= 2; 
            current->next = arena_create(new_size);
        }
        current = current->next;
    }
    
    void *ptr = current->buffer + current->offset;
    current->offset += aligned_size;
    return ptr;
}

char *arena_strdup(Arena *arena, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = arena_alloc(arena, len + 1);
    strcpy(copy, s);
    return copy;
}

void arena_free(Arena *arena) {
    Arena *current = arena;
    while (current) {
        Arena *next = current->next;
        free(current->buffer);
        free(current);
        current = next;
    }
}

void arena_reset(Arena *arena) {
    Arena *current = arena;
    while (current) {
        current->offset = 0;
        current = current->next;
    }
}
