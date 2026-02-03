// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef GC_H
#define GC_H

#include "value.h"

// Forward declaration for Env (defined in env.h)
typedef struct Env Env;

// Initialize GC with the global environment (our root)
void gc_init(Env *env);

// Core Allocator - Replaces malloc for Objects
// Allocates memory and adds the object to the tracking list
Obj *gc_allocate(size_t size, ObjType type);

// Trigger a garbage collection manually
void gc_collect(void);

// Clean up everything at exit
void gc_free_all(void);

// Helpers for the Mark phase
void mark_value(Value v);
void mark_obj(Obj *obj);

#endif