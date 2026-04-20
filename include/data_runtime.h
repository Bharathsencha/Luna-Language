// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_DATA_RUNTIME_H
#define LUNA_DATA_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

// Containment-hierarchy kind tags (match Rust RuntimeKind)
#define DATA_KIND_BLOC     1
#define DATA_KIND_BOX      2
#define DATA_KIND_TEMPLATE 3

// Rust FFI surface (from libluna_data_rt.a)
void luna_data_init(void);
int  luna_data_bloc_validate(const char *name, const char **fields, int count);
int  luna_data_box_size_ok(size_t size);
void luna_data_box_track(uint64_t handle);
int  luna_data_box_access_ok(uint64_t handle);
int  luna_data_box_free_ok(uint64_t handle);
int  luna_data_template_register(const char *name, const char **fields, int count);
int  luna_data_template_arity_ok(const char *name, int got);
int  luna_data_template_field_ok(const char *name, const char *field);
int  luna_data_containment_ok(int outer_kind, int inner_kind);
void luna_data_error_message(int code, char *buf, size_t buf_len);
void luna_data_shutdown(void);

// C bridge wrappers (call Rust + report errors)
void data_runtime_init(void);
void data_runtime_shutdown(void);

int  data_runtime_check_bloc(const char *name, const char **fields, int count, int line);
int  data_runtime_check_box_size(size_t size, int line);
void data_runtime_track_box(uint64_t handle);
int  data_runtime_check_box_access(uint64_t handle, int line);
int  data_runtime_check_box_free(uint64_t handle, int line);
int  data_runtime_check_template_register(const char *name, const char **fields, int count, int line);
int  data_runtime_check_template_arity(const char *name, int got, int line);
int  data_runtime_check_template_field(const char *name, const char *field, int line);
int  data_runtime_check_containment(int outer, int inner, int line);

#endif
