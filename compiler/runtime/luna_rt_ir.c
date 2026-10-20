// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// Pointer-ABI wrappers for the LLVM IR that lunac emits (compiler/emit_llvm.lu).
// Generated IR never passes an LV by value: it passes pointers to LV slots, so
// it does not depend on how the C ABI splits a 16-byte struct across registers.

#include "luna_rt.h"

#define BIN(name, fn) \
    void lvp_##name(LV *o, const LV *a, const LV *b) { LV x = *a, y = *b; *o = fn(x, y); }

BIN(add_fast, lv_add_fast)
BIN(sub_fast, lv_sub_fast)
BIN(mul_fast, lv_mul_fast)
BIN(div, lv_div)
BIN(mod, lv_mod)
BIN(eq_fast, lv_eq_fast)
BIN(ne_fast, lv_ne_fast)
BIN(lt_fast, lv_lt_fast)
BIN(le_fast, lv_le_fast)
BIN(gt_fast, lv_gt_fast)
BIN(ge_fast, lv_ge_fast)

void lvp_not(LV *o, const LV *a) { LV x = *a; *o = lv_not(x); }
int lvp_truth(const LV *a) { LV x = *a; return lv_truth(x); }
int lvp_eq_b(const LV *a, const LV *b) { LV x = *a, y = *b; return lv_eq(x, y).b; }

void lvp_index(LV *o, const LV *t, const LV *i) { LV x = *t, y = *i; *o = lv_index(x, y); }
void lvp_index_set(const LV *t, const LV *i, const LV *v) { LV x = *t, y = *i, z = *v; lv_index_set(x, y, z); }
void lvp_field(LV *o, const LV *t, const LV *key) { LV x = *t, k = *key; *o = lv_field(x, k); }
void lvp_map_put(const LV *m, const LV *key, const LV *v) { LV x = *m, k = *key, z = *v; lv_map_put(x, k.s, z); }

void lvp_container_check(const LV *v) { LV x = *v; lv_container_check(x); }
void lvp_global_store_check(const LV *v, int in_function) { LV x = *v; lv_global_store_check(x, in_function); }
void lvp_box_escape_check(const LV *v, const char *where) { LV x = *v; lv_box_escape_check(x, where); }
void lvp_box_new(LV *o, const LV *size, uint64_t scope_id) { LV x = *size; *o = lv_box_new(x, scope_id); }

void lvp_gget(LV *o, const LV *g, const char *name) { LV x = *g; *o = lv_gget(x, name); }

void lvp_iter_prepare(LV *o, const LV *v) { LV x = *v; *o = lv_iter_prepare(x); }
int64_t lvp_iter_len(const LV *v) { LV x = *v; return lv_iter_len(x); }
void lvp_iter_at(LV *o, const LV *v, int64_t i) { LV x = *v; *o = lv_iter_at(x, i); }

void lvp_call(LV *o, const LV *f, int argc, LV *argv) { LV x = *f; *o = lv_call(x, argc, argv); }
void lvp_print(const LV *v) { LV x = *v; lv_print(x); }
LV *lvp_cell(const LV *init) { LV x = *init; return lv_cell(x); }
