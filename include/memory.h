// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_MEMORY_H
#define LUNA_MEMORY_H

#include <stddef.h>
#include <stdint.h>

void luna_mem_init(void);
int luna_mem_begin(void);
void luna_mem_end(void);

void luna_mem_track_alloc(uintptr_t ptr, size_t size);
int luna_mem_free_ok(uintptr_t ptr);
int luna_mem_deref_ok(uintptr_t ptr);
int luna_mem_store_ok(uintptr_t ptr);
int luna_mem_ptr_add_ok(uintptr_t base, size_t offset, uintptr_t *out_ptr);
int luna_mem_addr_ok(uintptr_t ptr, int is_named);
int luna_mem_escape_ok(uintptr_t ptr);
int luna_mem_cmp_ok(uintptr_t ptr_a, uintptr_t ptr_b, int op);
int luna_mem_cast_ok(uintptr_t ptr, int target_type);
int luna_mem_call_ok(int is_builtin);
int luna_mem_alloc_size_ok(long n, int is_integer);
void luna_mem_defer(uintptr_t ptr);
void luna_mem_run_defers(void);
int luna_mem_gc_store_ok(uintptr_t ptr);
void luna_mem_error_message(int code, char *buf, size_t buf_len);
void luna_mem_shutdown(void);

#endif