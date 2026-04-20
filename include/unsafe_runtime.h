// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_UNSAFE_RUNTIME_H
#define LUNA_UNSAFE_RUNTIME_H

#include <stdint.h>
#include "value.h"

void unsafe_runtime_reset(void);
void unsafe_runtime_shutdown(void);

int unsafe_runtime_inside_block(void);
int unsafe_runtime_is_pointer(Value v);

int unsafe_runtime_begin_block(int line);
void unsafe_runtime_end_block(void);

int unsafe_runtime_check_gc_store(Value v, int line);
int unsafe_runtime_check_escape(Value v, int line);
int unsafe_runtime_check_compare(Value left, Value right, int op, int line);
int unsafe_runtime_check_cast(Value v, int target_type, int line);
int unsafe_runtime_check_call(int is_builtin, int line);

Value unsafe_runtime_alloc(Value size, int line);
Value unsafe_runtime_free(Value ptrv, int line);
Value unsafe_runtime_deref(Value ptrv, int line);
Value unsafe_runtime_store(Value ptrv, Value rhs, int line);
Value unsafe_runtime_ptr_add(Value basev, Value offv, int line);
Value unsafe_runtime_addr(Value *slot, int line);
Value unsafe_runtime_defer(Value ptrv, int line);
void unsafe_runtime_gc_mark_roots(void *ctx);

#endif
