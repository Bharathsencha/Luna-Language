// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// Runtime for C code emitted by lunac (compiler/lunac.lu).
// Values are tagged 16-byte structs. Heap objects are allocated through
// lv_alloc() and are not freed yet: memory management comes later.

#ifndef LUNA_RT_H
#define LUNA_RT_H

#include <stdint.h>
#include <stdio.h>

typedef enum {
    LV_NULL,
    LV_BOOL,
    LV_INT,
    LV_FLOAT,
    LV_CHAR,
    LV_STR,
    LV_LIST,
    LV_MAP,
    LV_FUNC,
    LV_FILE,
    LV_DENSE,             // contiguous array of doubles
    LV_BLOC_TYPE,         // bloc descriptor
    LV_BLOC,
    LV_DTYPE,             // data / template descriptor
    LV_TEMPLATE,
    LV_BOX,
    LV_PTR,               // raw pointer, only inside unsafe blocks
    LV_UNDEF              // a global that has not been defined yet
} LvTag;

typedef struct LvStr LvStr;
typedef struct LvList LvList;
typedef struct LvMap LvMap;
typedef struct LvFunc LvFunc;
typedef struct LvFile LvFile;
typedef struct LvDense LvDense;
typedef struct LvBlocDesc LvBlocDesc;
typedef struct LvBloc LvBloc;
typedef struct LvDType LvDType;
typedef struct LvTemplate LvTemplate;

typedef struct {
    char *buf;
    int64_t len;
    int64_t cap;
} LvBuf;

typedef struct LV {
    LvTag tag;
    union {
        int64_t i;
        double f;
        int b;
        char c;
        LvStr *s;
        LvList *l;
        LvMap *m;
        LvFunc *fn;
        LvFile *file;
        LvDense *d;
        LvBlocDesc *bd;
        LvBloc *bloc;
        LvDType *dt;
        LvTemplate *tpl;
        uint64_t box;
        uintptr_t ptr;
    };
} LV;

struct LvStr {
    int64_t len;
    char data[];          // NUL-terminated
};

struct LvList {
    int64_t count;
    int64_t cap;
    LV *items;
};

typedef struct {
    LvStr *key;
    LV val;
    int live;
} LvMapEntry;

struct LvMap {
    int64_t count;        // live entries
    int64_t used;         // entries[] slots used, including deleted ones
    int64_t cap;          // entries[] capacity
    LvMapEntry *entries;  // insertion order
    int64_t *index;       // hash slots: entry index + 1, 0 = empty, -1 = deleted
    int64_t icap;
};

typedef LV (*LvFnPtr)(LvFunc *self, int argc, LV *argv);

struct LvFunc {
    LvFnPtr fn;
    const char *name;
    int native;
    int ncells;
    LV **cells;           // captured variables, shared with the defining scope
};

struct LvDense {
    int64_t count;
    int64_t cap;
    double *data;
};

struct LvBloc {
    LvBlocDesc *desc;
    LV vals[];
};

struct LvFile {
    FILE *fp;
    int closed;
};

// A module's source, for error messages.
typedef struct {
    const char *path;
    const char *const *lines;
    int line_count;
} LvModule;

typedef enum {
    LV_ERR_RUNTIME,
    LV_ERR_TYPE,
    LV_ERR_NAME,
    LV_ERR_INDEX,
    LV_ERR_ARGUMENT,
    LV_ERR_ASSERTION
} LvErrKind;

// Current source position, updated once per statement.
extern const LvModule *lv_pos_mod;
extern int lv_pos_line;
extern int lv_depth;

// ---- constructors ----------------------------------------------------------
static inline LV lv_null(void) { LV v; v.tag = LV_NULL; v.i = 0; return v; }
static inline LV lv_bool(int b) { LV v; v.tag = LV_BOOL; v.i = 0; v.b = b != 0; return v; }
static inline LV lv_int(int64_t i) { LV v; v.tag = LV_INT; v.i = i; return v; }
static inline LV lv_float(double f) { LV v; v.tag = LV_FLOAT; v.f = f; return v; }
static inline LV lv_char(char c) { LV v; v.tag = LV_CHAR; v.i = 0; v.c = c; return v; }

void *lv_alloc(size_t n);            // zeroed, scanned by the collector
void *lv_alloc_atomic(size_t n);     // zeroed, never scanned (strings, doubles, bytes)
void *lv_realloc(void *p, size_t n);
void lv_gc_start(void);
void lv_gc_collect(void);
void lv_gc_root_add(void *lo, void *hi);
void lv_gc_root_remove(void *lo);
int lv_gc_is_marked(const void *p);
LV lv_str_n(const char *s, int64_t n);
LV lv_str(const char *s);
LV lv_list_new(int64_t cap);
LV lv_list_of(int n, const LV *items);
LV lv_map_new(void);
LV lv_dense_new(int64_t n);
LV lv_closure(LvFnPtr fn, const char *name, int ncells, LV **cells);
LV lv_native(LvFnPtr fn, const char *name);
LV *lv_cell(LV init);

// ---- data, template, bloc, box and unsafe (luna_rt_data.c) -----------------
LV lv_define_bloc(const char *name, int n, const char *const *fields);
LV lv_define_data(const char *name, int n, const char *const *fields, int is_template);
LV lv_construct(LV type, int argc, LV *argv);
uint64_t lv_scope_new(void);
LV lv_box_new(LV size, uint64_t scope_id);
void lv_box_release_scope(uint64_t scope_id);
void lv_box_escape_check(LV v, const char *where);
int lv_data_index(LV t, LV idx, LV *out);
int lv_data_field(LV t, LV key, LV *out);
int lv_data_index_set(LV t, LV idx, LV val);
int lv_data_equal(LV a, LV b);
int lv_data_len(LV v, int64_t *out);
void lv_data_tostring(LvBuf *b, LV v);
const char *lv_data_type_name(LV v);
void lv_data_init(void);
int lv_unsafe_depth(void);
void lv_unsafe_begin(void);
void lv_unsafe_end(void);
void lv_ptr_store_check(LV v);
void lv_container_check(LV v);
void lv_global_store_check(LV v, int in_function);
void lv_ptr_compare_check(LV a, LV b, int op);
void lv_ptr_cast_check(LV v, int target);
void lv_unsafe_call_check(int is_builtin);
int lv_is_pointer(LV v);
LV lv_addr(LV *slot);
LV lv_addr_invalid(int given);

// ---- errors ----------------------------------------------------------------
_Noreturn void lv_error(LvErrKind kind, const char *msg, const char *hint);
_Noreturn void lv_static_error(int line, const char *msg, const char *hint);
_Noreturn void lv_errorf(LvErrKind kind, const char *hint, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
const char *lv_type_name(LV v);
_Noreturn void lv_undefined(const char *name);

static inline LV lv_undef(void) { LV v; v.tag = LV_UNDEF; v.i = 0; return v; }
// Reads a global; reports a name error if it was never defined.
static inline LV lv_gget(LV v, const char *name) {
    if (v.tag == LV_UNDEF) lv_undefined(name);
    return v;
}

// ---- operators -------------------------------------------------------------
LV lv_add(LV a, LV b);
LV lv_sub(LV a, LV b);
LV lv_mul(LV a, LV b);
LV lv_div(LV a, LV b);
LV lv_mod(LV a, LV b);
LV lv_neg(LV a);
LV lv_eq(LV a, LV b);
LV lv_ne(LV a, LV b);
LV lv_lt(LV a, LV b);
LV lv_le(LV a, LV b);
LV lv_gt(LV a, LV b);
LV lv_ge(LV a, LV b);
LV lv_not(LV a);
int lv_truth(LV v);                  // condition: must be a boolean
int lv_equal(LV a, LV b);            // == without the type check

static inline LV lv_add_fast(LV a, LV b) {
    int64_t r;
    if (a.tag == LV_INT && b.tag == LV_INT && !__builtin_add_overflow(a.i, b.i, &r)) return lv_int(r);
    return lv_add(a, b);
}
static inline LV lv_sub_fast(LV a, LV b) {
    int64_t r;
    if (a.tag == LV_INT && b.tag == LV_INT && !__builtin_sub_overflow(a.i, b.i, &r)) return lv_int(r);
    return lv_sub(a, b);
}
static inline LV lv_mul_fast(LV a, LV b) {
    int64_t r;
    if (a.tag == LV_INT && b.tag == LV_INT && !__builtin_mul_overflow(a.i, b.i, &r)) return lv_int(r);
    return lv_mul(a, b);
}
static inline LV lv_lt_fast(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i < b.i);
    return lv_lt(a, b);
}
static inline LV lv_le_fast(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i <= b.i);
    return lv_le(a, b);
}
static inline LV lv_gt_fast(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i > b.i);
    return lv_gt(a, b);
}
static inline LV lv_ge_fast(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i >= b.i);
    return lv_ge(a, b);
}
static inline LV lv_eq_fast(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i == b.i);
    if (a.tag == LV_CHAR && b.tag == LV_CHAR) return lv_bool(a.c == b.c);
    return lv_eq(a, b);
}
static inline LV lv_ne_fast(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i != b.i);
    if (a.tag == LV_CHAR && b.tag == LV_CHAR) return lv_bool(a.c != b.c);
    return lv_ne(a, b);
}

// ---- containers ------------------------------------------------------------
LV lv_index(LV target, LV index);
void lv_index_set(LV target, LV index, LV val);
LV lv_field(LV target, LV key);            // target.name
void lv_list_push(LV list, LV v);
LV lv_map_get(LV map, LvStr *key, int *found);
void lv_map_put(LV map, LvStr *key, LV val);
int64_t lv_iter_len(LV v);
LV lv_iter_at(LV v, int64_t i);
LV lv_iter_prepare(LV v);                  // lists and strings only

// ---- strings ---------------------------------------------------------------
void lv_buf_init(LvBuf *b);
void lv_buf_add(LvBuf *b, const char *s, int64_t n);
void lv_buf_value(LvBuf *b, LV v);         // to_string rules
LV lv_buf_done(LvBuf *b);
LV lv_to_string(LV v);
LV lv_interp(int n, const LV *parts);      // "a{x}b"

// ---- calls -----------------------------------------------------------------
LV lv_call(LV f, int argc, LV *argv);
void lv_arity(LvFunc *self, int argc, int min, int max);
void lv_print(LV v);

// ---- program entry ---------------------------------------------------------
int lv_main(int argc, char **argv, void (*entry)(void));

// ---- builtins --------------------------------------------------------------
// Every builtin has the signature LV lb_<name>(LvFunc *self, int argc, LV *argv).
#define LV_BUILTIN(name) LV lb_##name(LvFunc *self, int argc, LV *argv)
LV_BUILTIN(len); LV_BUILTIN(str_len); LV_BUILTIN(append); LV_BUILTIN(list_append);
LV_BUILTIN(type); LV_BUILTIN(int); LV_BUILTIN(float); LV_BUILTIN(to_string);
LV_BUILTIN(to_int); LV_BUILTIN(to_float); LV_BUILTIN(assert); LV_BUILTIN(input);
LV_BUILTIN(debug); LV_BUILTIN(range); LV_BUILTIN(args); LV_BUILTIN(exit);
LV_BUILTIN(system); LV_BUILTIN(clock);
LV_BUILTIN(map_set); LV_BUILTIN(map_get); LV_BUILTIN(map_has); LV_BUILTIN(map_delete);
LV_BUILTIN(map_keys); LV_BUILTIN(map_values); LV_BUILTIN(map_items);
LV_BUILTIN(remove); LV_BUILTIN(find); LV_BUILTIN(slice); LV_BUILTIN(sort);
LV_BUILTIN(map); LV_BUILTIN(filter); LV_BUILTIN(reduce);
LV_BUILTIN(is_empty); LV_BUILTIN(concat); LV_BUILTIN(substring); LV_BUILTIN(char_at);
LV_BUILTIN(index_of); LV_BUILTIN(last_index_of); LV_BUILTIN(contains);
LV_BUILTIN(starts_with); LV_BUILTIN(ends_with); LV_BUILTIN(to_upper); LV_BUILTIN(to_lower);
LV_BUILTIN(trim); LV_BUILTIN(trim_left); LV_BUILTIN(trim_right); LV_BUILTIN(replace);
LV_BUILTIN(reverse); LV_BUILTIN(repeat); LV_BUILTIN(pad_left); LV_BUILTIN(pad_right);
LV_BUILTIN(format); LV_BUILTIN(split); LV_BUILTIN(join);
LV_BUILTIN(is_digit); LV_BUILTIN(is_alpha); LV_BUILTIN(is_alnum); LV_BUILTIN(is_space);
LV_BUILTIN(abs); LV_BUILTIN(min); LV_BUILTIN(max); LV_BUILTIN(clamp); LV_BUILTIN(sign);
LV_BUILTIN(pow); LV_BUILTIN(sqrt); LV_BUILTIN(cbrt); LV_BUILTIN(exp); LV_BUILTIN(ln);
LV_BUILTIN(log10); LV_BUILTIN(sin); LV_BUILTIN(cos); LV_BUILTIN(tan); LV_BUILTIN(asin);
LV_BUILTIN(acos); LV_BUILTIN(atan); LV_BUILTIN(atan2); LV_BUILTIN(sinh); LV_BUILTIN(cosh);
LV_BUILTIN(tanh); LV_BUILTIN(floor); LV_BUILTIN(ceil); LV_BUILTIN(round); LV_BUILTIN(trunc);
LV_BUILTIN(fract); LV_BUILTIN(mod); LV_BUILTIN(lerp); LV_BUILTIN(deg_to_rad);
LV_BUILTIN(rad_to_deg);
LV_BUILTIN(open); LV_BUILTIN(close); LV_BUILTIN(read); LV_BUILTIN(read_line);
LV_BUILTIN(dense_list); LV_BUILTIN(vec_add); LV_BUILTIN(vec_sub); LV_BUILTIN(vec_mul);
LV_BUILTIN(vec_div); LV_BUILTIN(vec_mul_inline); LV_BUILTIN(mat_mul);
LV_BUILTIN(rand); LV_BUILTIN(srand); LV_BUILTIN(trand); LV_BUILTIN(shuffle); LV_BUILTIN(ssort);
LV_BUILTIN(shape); LV_BUILTIN(alloc); LV_BUILTIN(free); LV_BUILTIN(load); LV_BUILTIN(deref);
LV_BUILTIN(store); LV_BUILTIN(ptr_offset); LV_BUILTIN(ptr_add); LV_BUILTIN(defer);
LV_BUILTIN(write); LV_BUILTIN(file_exists); LV_BUILTIN(remove_file); LV_BUILTIN(flush);

#endif
