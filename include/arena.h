// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct Arena Arena;

// Create a new memory arena with the given starting block size
Arena *arena_create(size_t size);

// Allocate memory from the arena
void *arena_alloc(Arena *arena, size_t size);

// Duplicate a string into the arena
char *arena_strdup(Arena *arena, const char *s);

// Free the entire arena and all its blocks
void arena_free(Arena *arena);

// Reset the arena offset without freeing the memory blocks
void arena_reset(Arena *arena);

#endif
