// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// Runtime for C code emitted by lunac. The rules follow the bytecode VM
// (src/luna_vm.c): strict operators, integer division, boolean conditions,
// and the same error format. Where the VM silently returns null for a bad
// operation, this runtime reports an error instead.

#define _GNU_SOURCE
#include "luna_rt.h"

#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

const LvModule *lv_pos_mod = NULL;
int lv_pos_line = 0;
int lv_depth = 0;

static int lv_argc = 0;
static char **lv_argv = NULL;

// ---- memory: see luna_gc.c ---------------------------------------------------

// ---- errors ----------------------------------------------------------------

static const char *lv_err_name(LvErrKind k) {
    switch (k) {
        case LV_ERR_RUNTIME: return "Runtime Error";
        case LV_ERR_TYPE: return "Type Error";
        case LV_ERR_NAME: return "Name Error";
        case LV_ERR_INDEX: return "Index Error";
        case LV_ERR_ARGUMENT: return "Argument Error";
        case LV_ERR_ASSERTION: return "Assertion Error";
    }
    return "Error";
}

void lv_error(LvErrKind kind, const char *msg, const char *hint) {
    fflush(stdout);
    const LvModule *m = lv_pos_mod;
    int line = lv_pos_line;
    fprintf(stderr, "%s", lv_err_name(kind));
    if (m && m->path) fprintf(stderr, " in %s", m->path);
    fprintf(stderr, " at line %d:\n  %s\n", line, msg);
    if (m && line >= 1 && line <= m->line_count) {
        fprintf(stderr, "\n%4d | %s\n\n", line, m->lines[line - 1]);
    }
    if (hint) fprintf(stderr, "Hint: %s\n", hint);
    exit(1);
}

// A lint error found before the program ran, reported the way the VM's parser does.
void lv_static_error(int line, const char *msg, const char *hint) {
    fflush(stdout);
    const LvModule *m = lv_pos_mod;
    fprintf(stderr, "Static Error");
    if (m && m->path) fprintf(stderr, " in %s", m->path);
    fprintf(stderr, " at line %d:\n  %s\n", line, msg);
    if (m && line >= 1 && line <= m->line_count) {
        fprintf(stderr, "\n%4d | %s\n\n", line, m->lines[line - 1]);
    }
    if (hint) fprintf(stderr, "Hint: %s\n", hint);
    fprintf(stderr, "Parsing failed.\n");
    exit(1);
}

void lv_errorf(LvErrKind kind, const char *hint, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    lv_error(kind, msg, hint);
}

void lv_undefined(const char *name) {
    lv_errorf(LV_ERR_NAME, "Declare variables with 'let' before using them",
              "Variable '%s' is not defined", name);
}

const char *lv_type_name(LV v) {
    switch (v.tag) {
        case LV_NULL: return "null";
        case LV_BOOL: return "boolean";
        case LV_INT: return "int";
        case LV_FLOAT: return "float";
        case LV_CHAR: return "char";
        case LV_STR: return "string";
        case LV_LIST: return "list";
        case LV_MAP: return "map";
        case LV_DENSE: return "list";
        case LV_BLOC_TYPE: case LV_BLOC: case LV_DTYPE: case LV_TEMPLATE: case LV_BOX: case LV_PTR:
            return lv_data_type_name(v);
        case LV_FUNC: return v.fn->native ? "native_function" : "function";
        case LV_FILE: return "file";
        case LV_UNDEF: return "undefined";
    }
    return "unknown";
}

// ---- constructors ----------------------------------------------------------

LV lv_str_n(const char *s, int64_t n) {
    LvStr *o = lv_alloc_atomic(sizeof(LvStr) + (size_t)n + 1);
    o->len = n;
    if (n) memcpy(o->data, s, (size_t)n);
    o->data[n] = '\0';
    LV v;
    v.tag = LV_STR;
    v.s = o;
    return v;
}

LV lv_str(const char *s) { return lv_str_n(s, (int64_t)strlen(s)); }

LV lv_list_new(int64_t cap) {
    LvList *l = lv_alloc(sizeof(LvList));
    l->count = 0;
    l->cap = cap < 4 ? 4 : cap;
    l->items = lv_alloc(sizeof(LV) * (size_t)l->cap);
    LV v;
    v.tag = LV_LIST;
    v.l = l;
    return v;
}

LV lv_list_of(int n, const LV *items) {
    LV v = lv_list_new(n);
    if (n) memcpy(v.l->items, items, sizeof(LV) * (size_t)n);
    v.l->count = n;
    return v;
}

void lv_list_push(LV list, LV v) {
    LvList *l = list.l;
    if (l->count == l->cap) {
        l->cap *= 2;
        l->items = lv_realloc(l->items, sizeof(LV) * (size_t)l->cap);
    }
    l->items[l->count++] = v;
}

LV lv_map_new(void) {
    LvMap *m = lv_alloc(sizeof(LvMap));
    m->count = 0;
    m->used = 0;
    m->cap = 8;
    m->entries = lv_alloc(sizeof(LvMapEntry) * (size_t)m->cap);
    m->icap = 16;
    m->index = lv_alloc_atomic(sizeof(int64_t) * (size_t)m->icap);
    LV v;
    v.tag = LV_MAP;
    v.m = m;
    return v;
}

LV lv_dense_new(int64_t n) {
    LvDense *d = lv_alloc(sizeof(LvDense));
    d->count = n;
    d->cap = n < 1 ? 1 : n;
    d->data = lv_alloc_atomic(sizeof(double) * (size_t)d->cap);
    LV v;
    v.tag = LV_DENSE;
    v.d = d;
    return v;
}

LV lv_closure(LvFnPtr fn, const char *name, int ncells, LV **cells) {
    LvFunc *f = lv_alloc(sizeof(LvFunc));
    f->fn = fn;
    f->name = name;
    f->native = 0;
    f->ncells = ncells;
    f->cells = NULL;
    if (ncells > 0) {
        f->cells = lv_alloc(sizeof(LV *) * (size_t)ncells);
        memcpy(f->cells, cells, sizeof(LV *) * (size_t)ncells);
    }
    LV v;
    v.tag = LV_FUNC;
    v.fn = f;
    return v;
}

LV lv_native(LvFnPtr fn, const char *name) {
    LV v = lv_closure(fn, name, 0, NULL);
    v.fn->native = 1;
    return v;
}

LV *lv_cell(LV init) {
    LV *c = lv_alloc(sizeof(LV));
    *c = init;
    return c;
}

// ---- maps ------------------------------------------------------------------

static uint64_t lv_hash(const char *s, int64_t n) {
    uint64_t h = 1469598103934665603ULL;
    for (int64_t i = 0; i < n; i++) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int lv_str_same(const LvStr *a, const LvStr *b) {
    return a == b || (a->len == b->len && memcmp(a->data, b->data, (size_t)a->len) == 0);
}

// Returns the hash slot holding key, or the slot to insert it into.
static int64_t lv_map_slot(LvMap *m, LvStr *key, int *found) {
    uint64_t mask = (uint64_t)m->icap - 1;
    uint64_t i = lv_hash(key->data, key->len) & mask;
    int64_t tomb = -1;
    for (;;) {
        int64_t e = m->index[i];
        if (e == 0) {
            *found = 0;
            return tomb >= 0 ? tomb : (int64_t)i;
        }
        if (e == -1) {
            if (tomb < 0) tomb = (int64_t)i;
        } else if (lv_str_same(m->entries[e - 1].key, key)) {
            *found = 1;
            return (int64_t)i;
        }
        i = (i + 1) & mask;
    }
}

static void lv_map_rehash(LvMap *m) {
    // Compact live entries, then rebuild the index.
    int64_t w = 0;
    for (int64_t r = 0; r < m->used; r++) {
        if (m->entries[r].live) m->entries[w++] = m->entries[r];
    }
    m->used = w;
    while (m->icap < (m->count + 1) * 2) m->icap *= 2;
    m->index = lv_alloc_atomic(sizeof(int64_t) * (size_t)m->icap);
    for (int64_t e = 0; e < m->used; e++) {
        int found;
        int64_t slot = lv_map_slot(m, m->entries[e].key, &found);
        m->index[slot] = e + 1;
    }
}

LV lv_map_get(LV map, LvStr *key, int *found) {
    LvMap *m = map.m;
    int f;
    int64_t slot = lv_map_slot(m, key, &f);
    if (found) *found = f;
    if (!f) return lv_null();
    return m->entries[m->index[slot] - 1].val;
}

void lv_map_put(LV map, LvStr *key, LV val) {
    LvMap *m = map.m;
    int found;
    int64_t slot = lv_map_slot(m, key, &found);
    if (found) {
        m->entries[m->index[slot] - 1].val = val;
        return;
    }
    if (m->used == m->cap) {
        if (m->count < m->used / 2) {
            lv_map_rehash(m);
        } else {
            m->cap *= 2;
            m->entries = lv_realloc(m->entries, sizeof(LvMapEntry) * (size_t)m->cap);
        }
        slot = lv_map_slot(m, key, &found);
    }
    m->entries[m->used].key = key;
    m->entries[m->used].val = val;
    m->entries[m->used].live = 1;
    m->used++;
    m->count++;
    m->index[slot] = m->used;
    if (m->used * 2 > m->icap) lv_map_rehash(m);
}

static int lv_map_del(LV map, LvStr *key) {
    LvMap *m = map.m;
    int found;
    int64_t slot = lv_map_slot(m, key, &found);
    if (!found) return 0;
    m->entries[m->index[slot] - 1].live = 0;
    m->index[slot] = -1;
    m->count--;
    return 1;
}

// ---- strings ---------------------------------------------------------------

void lv_buf_init(LvBuf *b) {
    b->cap = 64;
    b->len = 0;
    b->buf = lv_alloc_atomic((size_t)b->cap);
}

void lv_buf_add(LvBuf *b, const char *s, int64_t n) {
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->buf = lv_realloc(b->buf, (size_t)b->cap);
    }
    memcpy(b->buf + b->len, s, (size_t)n);
    b->len += n;
}

static void lv_buf_cstr(LvBuf *b, const char *s) { lv_buf_add(b, s, (int64_t)strlen(s)); }

void lv_buf_value(LvBuf *b, LV v) {
    char tmp[64];
    switch (v.tag) {
        case LV_NULL: lv_buf_cstr(b, "null"); break;
        case LV_BOOL: lv_buf_cstr(b, v.b ? "true" : "false"); break;
        case LV_INT: snprintf(tmp, sizeof(tmp), "%lld", (long long)v.i); lv_buf_cstr(b, tmp); break;
        case LV_FLOAT: snprintf(tmp, sizeof(tmp), "%.6g", v.f); lv_buf_cstr(b, tmp); break;
        case LV_CHAR: lv_buf_add(b, &v.c, 1); break;
        case LV_STR: lv_buf_add(b, v.s->data, v.s->len); break;
        case LV_LIST:
            lv_buf_cstr(b, "[");
            for (int64_t i = 0; i < v.l->count; i++) {
                if (i) lv_buf_cstr(b, ", ");
                lv_buf_value(b, v.l->items[i]);
            }
            lv_buf_cstr(b, "]");
            break;
        case LV_MAP: {
            lv_buf_cstr(b, "{");
            int first = 1;
            for (int64_t i = 0; i < v.m->used; i++) {
                LvMapEntry *e = &v.m->entries[i];
                if (!e->live) continue;
                if (!first) lv_buf_cstr(b, ", ");
                first = 0;
                lv_buf_cstr(b, "\"");
                lv_buf_add(b, e->key->data, e->key->len);
                lv_buf_cstr(b, "\": ");
                lv_buf_value(b, e->val);
            }
            lv_buf_cstr(b, "}");
            break;
        }
        case LV_DENSE:
            lv_buf_cstr(b, "d[");
            for (int64_t i = 0; i < v.d->count; i++) {
                if (i) lv_buf_cstr(b, ", ");
                snprintf(tmp, sizeof(tmp), "%.6g", v.d->data[i]);
                lv_buf_cstr(b, tmp);
            }
            lv_buf_cstr(b, "]");
            break;
        case LV_FUNC: lv_buf_cstr(b, v.fn->native ? "<native function>" : "<function>"); break;
        case LV_FILE: lv_buf_cstr(b, "<file>"); break;
        case LV_BLOC_TYPE: case LV_BLOC: case LV_DTYPE: case LV_TEMPLATE: case LV_BOX: case LV_PTR:
            lv_data_tostring(b, v);
            break;
        case LV_UNDEF: lv_buf_cstr(b, "<undefined>"); break;
    }
}

LV lv_buf_done(LvBuf *b) {
    LV v = lv_str_n(b->buf, b->len);
    b->buf = NULL;
    return v;
}

LV lv_to_string(LV v) {
    if (v.tag == LV_STR) return v;
    LvBuf b;
    lv_buf_init(&b);
    lv_buf_value(&b, v);
    return lv_buf_done(&b);
}

LV lv_interp(int n, const LV *parts) {
    LvBuf b;
    lv_buf_init(&b);
    for (int i = 0; i < n; i++) lv_buf_value(&b, parts[i]);
    return lv_buf_done(&b);
}

void lv_print(LV v) {
    if (v.tag == LV_STR) {
        fwrite(v.s->data, 1, (size_t)v.s->len, stdout);
        fputc('\n', stdout);
        return;
    }
    LvBuf b;
    lv_buf_init(&b);
    lv_buf_value(&b, v);
    lv_buf_add(&b, "\n", 1);
    fwrite(b.buf, 1, (size_t)b.len, stdout);
}

// ---- operators -------------------------------------------------------------

static int lv_is_num(LV v) { return v.tag == LV_INT || v.tag == LV_FLOAT; }
static double lv_num(LV v) { return v.tag == LV_INT ? (double)v.i : v.f; }

__attribute__((cold, noreturn)) static void lv_operand_error(const char *op, LV a, LV b) {
    char msg[160];
    snprintf(msg, sizeof(msg), "cannot use '%s' on %s and %s", op, lv_type_name(a), lv_type_name(b));
    lv_error(LV_ERR_TYPE, msg, op[0] == '+' && op[1] == '\0'
        ? "Both sides must be numbers, or both strings. Build text with \"{x}\" or to_string(x)."
        : "Both sides must be numbers.");
}

__attribute__((cold, noreturn)) static void lv_overflow(const char *op) {
    lv_errorf(LV_ERR_RUNTIME,
        "The result does not fit in a 64-bit int. Use floats for very large numbers.",
        "integer overflow in '%s'", op);
}

__attribute__((cold, noreturn)) static void lv_div_zero(void) {
    lv_error(LV_ERR_RUNTIME, "division by zero", "Check the divisor before dividing.");
}

static double lv_elem(LV v) { return v.tag == LV_INT ? (double)v.i : v.tag == LV_FLOAT ? v.f : 0.0; }

// Element-wise arithmetic on two lists or two dense lists, as in the VM: the
// result is a dense list (an empty list when two empty plain lists meet), and
// a mix of a list with a dense list gives null. Division follows IEEE rules.
static LV lv_list_arith(int op, LV a, LV b) {
    int64_t n;
    if (a.tag == LV_DENSE && b.tag == LV_DENSE) {
        n = a.d->count < b.d->count ? a.d->count : b.d->count;
        if (n == 0) return lv_dense_new(0);
    } else if (a.tag == LV_LIST && b.tag == LV_LIST) {
        n = a.l->count < b.l->count ? a.l->count : b.l->count;
        if (n == 0) return lv_list_new(0);
    } else {
        return lv_null();
    }
    LV out = lv_dense_new(n);
    for (int64_t i = 0; i < n; i++) {
        double x = a.tag == LV_DENSE ? a.d->data[i] : lv_elem(a.l->items[i]);
        double y = b.tag == LV_DENSE ? b.d->data[i] : lv_elem(b.l->items[i]);
        out.d->data[i] = op == 0 ? x + y : op == 1 ? x - y : op == 2 ? x * y : x / y;
    }
    return out;
}

static int lv_is_listish(LV v) { return v.tag == LV_LIST || v.tag == LV_DENSE; }

LV lv_add(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) {
        int64_t r;
        if (__builtin_add_overflow(a.i, b.i, &r)) lv_overflow("+");
        return lv_int(r);
    }
    if (lv_is_num(a) && lv_is_num(b)) return lv_float(lv_num(a) + lv_num(b));
    if (a.tag == LV_STR && b.tag == LV_STR) {
        LV v = lv_str_n(a.s->data, a.s->len + b.s->len);
        memcpy(v.s->data + a.s->len, b.s->data, (size_t)b.s->len);
        v.s->data[v.s->len] = '\0';
        return v;
    }
    if (lv_is_listish(a) && lv_is_listish(b)) return lv_list_arith(0, a, b);
    lv_operand_error("+", a, b);
}

LV lv_sub(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) {
        int64_t r;
        if (__builtin_sub_overflow(a.i, b.i, &r)) lv_overflow("-");
        return lv_int(r);
    }
    if (lv_is_num(a) && lv_is_num(b)) return lv_float(lv_num(a) - lv_num(b));
    if (lv_is_listish(a) && lv_is_listish(b)) return lv_list_arith(1, a, b);
    lv_operand_error("-", a, b);
}

LV lv_mul(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) {
        int64_t r;
        if (__builtin_mul_overflow(a.i, b.i, &r)) lv_overflow("*");
        return lv_int(r);
    }
    if (lv_is_num(a) && lv_is_num(b)) return lv_float(lv_num(a) * lv_num(b));
    if (lv_is_listish(a) && lv_is_listish(b)) return lv_list_arith(2, a, b);
    lv_operand_error("*", a, b);
}

LV lv_div(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) {
        if (b.i == 0) lv_div_zero();
        if (a.i == INT64_MIN && b.i == -1) lv_overflow("/");
        return lv_int(a.i / b.i);
    }
    if (lv_is_num(a) && lv_is_num(b)) {
        double d = lv_num(b);
        if (d == 0.0) lv_div_zero();
        return lv_float(lv_num(a) / d);
    }
    if (lv_is_listish(a) && lv_is_listish(b)) return lv_list_arith(3, a, b);
    lv_operand_error("/", a, b);
}

LV lv_mod(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) {
        if (b.i == 0) lv_div_zero();
        if (b.i == -1) return lv_int(0);
        return lv_int(a.i % b.i);
    }
    if (lv_is_num(a) && lv_is_num(b)) {
        double d = lv_num(b);
        if (d == 0.0) lv_div_zero();
        return lv_float(fmod(lv_num(a), d));
    }
    lv_operand_error("%", a, b);
}

LV lv_neg(LV a) {
    if (a.tag == LV_INT) {
        if (a.i == INT64_MIN) lv_overflow("-");
        return lv_int(-a.i);
    }
    if (a.tag == LV_FLOAT) return lv_float(-a.f);
    lv_operand_error("-", lv_int(0), a);
}

static int lv_family(LV v) {
    switch (v.tag) {
        case LV_INT: case LV_FLOAT: return 1;
        case LV_LIST: case LV_DENSE: return 2;
        default: return 100 + (int)v.tag;
    }
}

int lv_equal(LV a, LV b) {
    if (a.tag == LV_NULL || b.tag == LV_NULL) return a.tag == b.tag;
    if (a.tag == LV_INT && b.tag == LV_INT) return a.i == b.i;
    if (lv_is_num(a) && lv_is_num(b)) return lv_num(a) == lv_num(b);
    if (a.tag != b.tag) return 0;
    switch (a.tag) {
        case LV_BOOL: return a.b == b.b;
        case LV_CHAR: return a.c == b.c;
        case LV_STR: return lv_str_same(a.s, b.s);
        case LV_LIST: return a.l == b.l;
        case LV_DENSE: return a.d == b.d;
        case LV_BLOC_TYPE: case LV_BLOC: case LV_DTYPE: case LV_TEMPLATE: case LV_BOX: case LV_PTR:
            return lv_data_equal(a, b);
        case LV_MAP: return a.m == b.m;
        case LV_FUNC: return a.fn == b.fn || (a.fn->native && b.fn->native && a.fn->fn == b.fn->fn);
        case LV_FILE: return a.file == b.file;
        default: return 0;
    }
}

static void lv_check_comparable(LV a, LV b) {
    if (a.tag == LV_NULL || b.tag == LV_NULL) return;
    if (lv_family(a) != lv_family(b)) {
        lv_errorf(LV_ERR_TYPE,
            "Compare values of the same type (comparing with null is always allowed).",
            "cannot compare %s with %s", lv_type_name(a), lv_type_name(b));
    }
}

LV lv_eq(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i == b.i);
    if (a.tag == LV_PTR) lv_ptr_compare_check(a, b, 0);
    lv_check_comparable(a, b);
    return lv_bool(lv_equal(a, b));
}

LV lv_ne(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return lv_bool(a.i != b.i);
    if (a.tag == LV_PTR) lv_ptr_compare_check(a, b, 1);
    lv_check_comparable(a, b);
    return lv_bool(!lv_equal(a, b));
}

static int lv_order(LV a, LV b) {
    if (a.tag == LV_INT && b.tag == LV_INT) return (a.i > b.i) - (a.i < b.i);
    if (lv_is_num(a) && lv_is_num(b)) {
        double x = lv_num(a), y = lv_num(b);
        return (x > y) - (x < y);
    }
    if (a.tag == LV_STR && b.tag == LV_STR) {
        int c = strcmp(a.s->data, b.s->data);
        return (c > 0) - (c < 0);
    }
    if (a.tag == LV_CHAR && b.tag == LV_CHAR) return (a.c > b.c) - (a.c < b.c);
    if (a.tag == LV_PTR && b.tag == LV_PTR) return (a.ptr > b.ptr) - (a.ptr < b.ptr);
    lv_errorf(LV_ERR_TYPE, "Ordering works on two numbers, two strings or two chars.",
              "cannot order %s and %s with <, <=, > or >=", lv_type_name(a), lv_type_name(b));
}

LV lv_lt(LV a, LV b) { if (a.tag == LV_PTR) lv_ptr_compare_check(a, b, 2); return lv_bool(lv_order(a, b) < 0); }
LV lv_le(LV a, LV b) { if (a.tag == LV_PTR) lv_ptr_compare_check(a, b, 4); return lv_bool(lv_order(a, b) <= 0); }
LV lv_gt(LV a, LV b) { if (a.tag == LV_PTR) lv_ptr_compare_check(a, b, 3); return lv_bool(lv_order(a, b) > 0); }
LV lv_ge(LV a, LV b) { if (a.tag == LV_PTR) lv_ptr_compare_check(a, b, 5); return lv_bool(lv_order(a, b) >= 0); }

int lv_truth(LV v) {
    if (v.tag == LV_BOOL) return v.b;
    lv_errorf(LV_ERR_TYPE,
        "Write the test explicitly, for example x != 0, len(s) > 0 or v != null.",
        "condition must be a boolean, got %s", lv_type_name(v));
}

LV lv_not(LV a) { return lv_bool(!lv_truth(a)); }

// ---- indexing --------------------------------------------------------------

__attribute__((cold, noreturn)) static void lv_oob(int64_t idx, int64_t len, const char *what) {
    lv_errorf(LV_ERR_INDEX, "Ensure your index is between 0 and len-1",
              "Index %lld is out of bounds for %s of length %lld", (long long)idx, what, (long long)len);
}

LV lv_index(LV t, LV idx) {
    LV got;
    if ((t.tag >= LV_BLOC_TYPE && t.tag <= LV_PTR) && lv_data_index(t, idx, &got)) return got;
    if (t.tag == LV_LIST) {
        if (idx.tag != LV_INT) lv_errorf(LV_ERR_TYPE, "Use an int index", "list index must be an int, got %s", lv_type_name(idx));
        int64_t i = idx.i;
        if (i < 0) i += t.l->count;
        if (i < 0 || i >= t.l->count) lv_oob(idx.i, t.l->count, "list");
        return t.l->items[i];
    }
    if (t.tag == LV_DENSE) {
        if (idx.tag != LV_INT) lv_errorf(LV_ERR_TYPE, "Use an int index", "list index must be an int, got %s", lv_type_name(idx));
        int64_t i = idx.i;
        if (i < 0) i += t.d->count;
        if (i < 0 || i >= t.d->count) lv_oob(idx.i, t.d->count, "list");
        return lv_float(t.d->data[i]);
    }
    if (t.tag == LV_STR) {
        if (idx.tag != LV_INT) lv_errorf(LV_ERR_TYPE, "Use an int index", "string index must be an int, got %s", lv_type_name(idx));
        int64_t i = idx.i;
        if (i < 0) i += t.s->len;
        if (i < 0 || i >= t.s->len) lv_oob(idx.i, t.s->len, "string");
        return lv_char(t.s->data[i]);
    }
    if (t.tag == LV_MAP) {
        if (idx.tag != LV_STR) lv_errorf(LV_ERR_TYPE, "Maps use string keys", "map key must be a string, got %s", lv_type_name(idx));
        return lv_map_get(t, idx.s, NULL);
    }
    lv_errorf(LV_ERR_TYPE, "Index lists and strings with ints, maps with strings",
              "cannot index %s", lv_type_name(t));
}

LV lv_field(LV t, LV key) {
    LV got;
    if (t.tag == LV_MAP) return lv_map_get(t, key.s, NULL);
    if ((t.tag >= LV_BLOC_TYPE && t.tag <= LV_PTR) && lv_data_field(t, key, &got)) return got;
    lv_errorf(LV_ERR_TYPE, "Dot access works on maps", "cannot read field '%s' of %s",
              key.s->data, lv_type_name(t));
}

void lv_index_set(LV t, LV idx, LV val) {
    if ((t.tag >= LV_BLOC_TYPE && t.tag <= LV_PTR) && lv_data_index_set(t, idx, val)) return;
    if (t.tag == LV_LIST || t.tag == LV_MAP) lv_container_check(val);
    if (t.tag == LV_LIST) {
        if (idx.tag != LV_INT) lv_errorf(LV_ERR_TYPE, "Use an int index", "list index must be an int, got %s", lv_type_name(idx));
        int64_t i = idx.i;
        if (i < 0) i += t.l->count;
        if (i < 0 || i >= t.l->count) {
            lv_errorf(LV_ERR_INDEX, "Ensure your index is between 0 and len(list)-1",
                      "Index %lld is out of bounds for list of length %lld",
                      (long long)idx.i, (long long)t.l->count);
        }
        t.l->items[i] = val;
        return;
    }
    if (t.tag == LV_DENSE) {
        if (idx.tag != LV_INT) lv_errorf(LV_ERR_TYPE, "Use an int index", "list index must be an int, got %s", lv_type_name(idx));
        int64_t i = idx.i;
        if (i < 0) i += t.d->count;
        if (i < 0 || i >= t.d->count) {
            lv_errorf(LV_ERR_INDEX, "Ensure your index is between 0 and len(list)-1",
                      "Index %lld is out of bounds for list of length %lld",
                      (long long)idx.i, (long long)t.d->count);
        }
        t.d->data[i] = lv_elem(val);
        return;
    }
    if (t.tag == LV_MAP) {
        if (idx.tag != LV_STR) lv_errorf(LV_ERR_TYPE, "Maps use string keys", "map key must be a string, got %s", lv_type_name(idx));
        lv_map_put(t, idx.s, val);
        return;
    }
    lv_error(LV_ERR_TYPE, "Cannot assign through this target",
             "Use integer indices for lists, or string keys for maps and templates");
}

// for-in: lists by element, strings by char. Maps are rejected, as in the
// interpreter; iterate map_keys(m) instead.
LV lv_iter_prepare(LV v) {
    if (v.tag == LV_LIST || v.tag == LV_STR || v.tag == LV_DENSE) return v;
    lv_error(LV_ERR_TYPE, "for-in expects a list, dense list, or string",
             "Use for (let item in list) { ... }");
}

int64_t lv_iter_len(LV v) {
    return v.tag == LV_LIST ? v.l->count : v.tag == LV_DENSE ? v.d->count : v.s->len;
}

LV lv_iter_at(LV v, int64_t i) {
    int64_t n = lv_iter_len(v);
    if (i >= n) lv_oob(i, n, v.tag == LV_STR ? "string" : "list");
    if (v.tag == LV_DENSE) return lv_float(v.d->data[i]);
    return v.tag == LV_LIST ? v.l->items[i] : lv_char(v.s->data[i]);
}

// ---- calls -----------------------------------------------------------------

#define LV_MAX_DEPTH 100000

LV lv_call(LV f, int argc, LV *argv) {
    if (f.tag == LV_DTYPE || f.tag == LV_BLOC_TYPE) return lv_construct(f, argc, argv);
    if (f.tag != LV_FUNC) {
        lv_errorf(LV_ERR_TYPE, "Make sure you are calling a function value, closure, or native builtin",
                  "cannot call %s", lv_type_name(f));
    }
    const LvModule *mod = lv_pos_mod;
    int line = lv_pos_line;
    if (lv_unsafe_depth() > 0) lv_unsafe_call_check(f.fn->native);
    if (++lv_depth > LV_MAX_DEPTH) {
        lv_error(LV_ERR_RUNTIME, "stack overflow: recursion is too deep",
                 "Luna allows 100000 nested calls. Check for a recursion that never stops.");
    }
    LV r = f.fn->fn(f.fn, argc, argv);
    lv_depth--;
    lv_pos_mod = mod;
    lv_pos_line = line;
    return r;
}

void lv_arity(LvFunc *self, int argc, int min, int max) {
    if (argc >= min && argc <= max) return;
    const char *name = self && self->name ? self->name : "function";
    if (min == max) {
        lv_errorf(LV_ERR_ARGUMENT, "Pass one value per parameter",
                  "%s() expects %d argument%s, but got %d", name, min, min == 1 ? "" : "s", argc);
    }
    lv_errorf(LV_ERR_ARGUMENT, "Pass one value per required parameter",
              "%s() expects %d to %d arguments, but got %d", name, min, max, argc);
}

// ---- entry -----------------------------------------------------------------

typedef struct {
    void (*entry)(void);
} LvStart;

static void *lv_thread_main(void *arg) {
    LvStart *s = arg;
    lv_gc_start();
    s->entry();
    fflush(stdout);
    return NULL;
}

int lv_main(int argc, char **argv, void (*entry)(void)) {
    lv_data_init();
    lb_srand(NULL, 0, NULL);
    lv_argc = argc;
    lv_argv = argv;
    // Deep recursion needs a large stack; run the program on its own thread.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (size_t)1 << 30);
    pthread_t th;
    LvStart s = { entry };
    if (pthread_create(&th, &attr, lv_thread_main, &s) != 0) {
        lv_gc_start();
        entry();
    } else {
        pthread_join(th, NULL);
    }
    fflush(stdout);
    return 0;
}

// ---- builtin helpers -------------------------------------------------------

static void lv_need(LvFunc *self, int argc, int min, int max) { lv_arity(self, argc, min, max); }

static const char *lv_fname(LvFunc *self) { return self && self->name ? self->name : "builtin"; }

static LvStr *lv_arg_str(LvFunc *self, LV *argv, int i) {
    if (argv[i].tag != LV_STR) {
        lv_errorf(LV_ERR_TYPE, NULL, "%s() argument %d must be a string, got %s",
                  lv_fname(self), i + 1, lv_type_name(argv[i]));
    }
    return argv[i].s;
}

static int64_t lv_arg_int(LvFunc *self, LV *argv, int i) {
    if (argv[i].tag != LV_INT) {
        lv_errorf(LV_ERR_TYPE, NULL, "%s() argument %d must be an int, got %s",
                  lv_fname(self), i + 1, lv_type_name(argv[i]));
    }
    return argv[i].i;
}

static double lv_arg_num(LvFunc *self, LV *argv, int i) {
    if (!lv_is_num(argv[i])) {
        lv_errorf(LV_ERR_TYPE, NULL, "%s() argument %d must be a number, got %s",
                  lv_fname(self), i + 1, lv_type_name(argv[i]));
    }
    return lv_num(argv[i]);
}

static LvList *lv_arg_list(LvFunc *self, LV *argv, int i) {
    if (argv[i].tag != LV_LIST) {
        lv_errorf(LV_ERR_TYPE, NULL, "%s() argument %d must be a list, got %s",
                  lv_fname(self), i + 1, lv_type_name(argv[i]));
    }
    return argv[i].l;
}

static LV lv_arg_map(LvFunc *self, LV *argv, int i) {
    if (argv[i].tag != LV_MAP) {
        lv_errorf(LV_ERR_TYPE, NULL, "%s() argument %d must be a map, got %s",
                  lv_fname(self), i + 1, lv_type_name(argv[i]));
    }
    return argv[i];
}

// A char argument counts as a one-character string.
static LV lv_textish(LV v) {
    if (v.tag == LV_CHAR) return lv_str_n(&v.c, 1);
    return v;
}

// ---- core builtins ---------------------------------------------------------

LV_BUILTIN(len) {
    lv_need(self, argc, 1, 1);
    switch (argv[0].tag) {
        case LV_STR: return lv_int(argv[0].s->len);
        case LV_LIST: return lv_int(argv[0].l->count);
        case LV_DENSE: return lv_int(argv[0].d->count);
        case LV_MAP: return lv_int(argv[0].m->count);
        case LV_BOX: case LV_TEMPLATE: {
            int64_t n;
            lv_data_len(argv[0], &n);
            return lv_int(n);
        }
        default:
            lv_errorf(LV_ERR_TYPE, NULL, "len() needs a string, list or map, got %s", lv_type_name(argv[0]));
    }
}

LV_BUILTIN(str_len) { return lb_len(self, argc, argv); }

LV_BUILTIN(append) {
    lv_need(self, argc, 2, 2);
    lv_arg_list(self, argv, 0);
    lv_list_push(argv[0], argv[1]);
    return lv_null();
}

LV_BUILTIN(list_append) { return lb_append(self, argc, argv); }

LV_BUILTIN(type) {
    lv_need(self, argc, 1, 1);
    if (argv[0].tag == LV_INT && (argv[0].i > INT32_MAX || argv[0].i < INT32_MIN)) return lv_str("long");
    return lv_str(lv_type_name(argv[0]));
}

static int lv_parse_int(const char *s, int64_t *out) {
    while (isspace((unsigned char)*s)) s++;
    char *end;
    long long v = strtoll(s, &end, 10);
    if (end == s) return 0;
    *out = v;
    return 1;
}

LV_BUILTIN(int) {
    lv_need(self, argc, 1, 1);
    LV v = argv[0];
    if (v.tag == LV_PTR && lv_is_pointer(v)) return lv_int((int64_t)v.ptr);
    switch (v.tag) {
        case LV_INT: return v;
        case LV_FLOAT:
            if (!(v.f > -9.3e18 && v.f < 9.3e18)) lv_error(LV_ERR_RUNTIME, "int() of a float that does not fit in 64 bits", NULL);
            return lv_int((int64_t)v.f);
        case LV_CHAR: return lv_int((unsigned char)v.c);
        case LV_BOOL: return lv_int(v.b);
        case LV_STR: {
            int64_t out = 0;
            lv_parse_int(v.s->data, &out);
            return lv_int(out);
        }
        default:
            lv_errorf(LV_ERR_TYPE, NULL, "int() cannot convert %s", lv_type_name(v));
    }
}

LV_BUILTIN(float) {
    lv_need(self, argc, 1, 1);
    LV v = argv[0];
    lv_ptr_cast_check(v, 1);
    switch (v.tag) {
        case LV_INT: return lv_float((double)v.i);
        case LV_FLOAT: return v;
        case LV_CHAR: return lv_float((double)(unsigned char)v.c);
        case LV_BOOL: return lv_float(v.b);
        case LV_STR: return lv_float(atof(v.s->data));
        default:
            lv_errorf(LV_ERR_TYPE, NULL, "float() cannot convert %s", lv_type_name(v));
    }
}

LV_BUILTIN(to_string) {
    lv_need(self, argc, 1, 1);
    return lv_to_string(argv[0]);
}

LV_BUILTIN(to_int) {
    lv_need(self, argc, 1, 1);
    if (argv[0].tag != LV_STR) return lb_int(self, argc, argv);
    int64_t out = 0;
    lv_parse_int(argv[0].s->data, &out);
    return lv_int(out);
}

LV_BUILTIN(to_float) {
    lv_need(self, argc, 1, 1);
    return lb_float(self, argc, argv);
}

// assert() follows the VM: it accepts any value, using its truthiness.
static int lv_assert_truthy(LV v) {
    switch (v.tag) {
        case LV_BOOL: return v.b;
        case LV_INT: return v.i != 0;
        case LV_FLOAT: return v.f != 0.0;
        case LV_CHAR: return v.c != 0;
        case LV_STR: return v.s->len > 0;
        case LV_NULL: return 0;
        case LV_FILE: return v.file != NULL && !v.file->closed;
        case LV_LIST: case LV_DENSE: case LV_MAP: case LV_FUNC: return 1;
        case LV_BLOC_TYPE: case LV_BLOC: case LV_DTYPE: case LV_TEMPLATE: case LV_BOX: return 1;
        case LV_PTR: return v.ptr != 0;
        default: return 0;
    }
}

LV_BUILTIN(assert) {
    if (argc != 1) {
        lv_error(LV_ERR_ARGUMENT, "assert() takes exactly 1 argument",
                 "Use assert(condition) to verify logic.");
    }
    if (!lv_assert_truthy(argv[0])) {
        lv_error(LV_ERR_ASSERTION, "Assertion failed", "The condition evaluated to false.");
    }
    return lv_bool(1);
}

LV_BUILTIN(input) {
    lv_need(self, argc, 0, 1);
    if (argc == 1) {
        LV p = lv_to_string(argv[0]);
        fwrite(p.s->data, 1, (size_t)p.s->len, stdout);
        fflush(stdout);
    }
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) {
        free(line);
        return lv_str("");
    }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) n--;
    LV r = lv_str_n(line, n);
    free(line);
    return r;
}

LV_BUILTIN(debug) {
    lv_need(self, argc, 1, 1);
    LV s = lv_to_string(argv[0]);
    printf("value = %s\n", s.s->data);
    return argv[0];
}

LV_BUILTIN(range) {
    lv_need(self, argc, 1, 3);
    int64_t start = 0, end, step = 1;
    if (argc == 1) {
        end = lv_arg_int(self, argv, 0);
    } else {
        start = lv_arg_int(self, argv, 0);
        end = lv_arg_int(self, argv, 1);
        if (argc == 3) step = lv_arg_int(self, argv, 2);
    }
    if (step == 0) lv_error(LV_ERR_ARGUMENT, "range() step must not be 0", NULL);
    int64_t n = 0;
    if (step > 0 && end > start) n = (end - start + step - 1) / step;
    if (step < 0 && end < start) n = (start - end + (-step) - 1) / (-step);
    LV out = lv_list_new(n);
    for (int64_t i = 0, v = start; i < n; i++, v += step) out.l->items[i] = lv_int(v);
    out.l->count = n;
    return out;
}

LV_BUILTIN(args) {
    lv_need(self, argc, 0, 0);
    LV out = lv_list_new(lv_argc);
    for (int i = 1; i < lv_argc; i++) lv_list_push(out, lv_str(lv_argv[i]));
    return out;
}

LV_BUILTIN(exit) {
    lv_need(self, argc, 0, 1);
    int code = argc == 1 ? (int)lv_arg_int(self, argv, 0) : 0;
    fflush(stdout);
    exit(code);
}

LV_BUILTIN(system) {
    lv_need(self, argc, 1, 1);
    LvStr *cmd = lv_arg_str(self, argv, 0);
    fflush(stdout);
    int rc = system(cmd->data);
    if (rc == -1) return lv_int(-1);
    if (WIFEXITED(rc)) return lv_int(WEXITSTATUS(rc));
    return lv_int(128 + (WIFSIGNALED(rc) ? WTERMSIG(rc) : 0));
}

LV_BUILTIN(clock) {
    lv_need(self, argc, 0, 0);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return lv_float((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
}

// ---- maps ------------------------------------------------------------------

LV_BUILTIN(map_set) {
    lv_need(self, argc, 3, 3);
    LV m = lv_arg_map(self, argv, 0);
    lv_map_put(m, lv_arg_str(self, argv, 1), argv[2]);
    return lv_null();
}

LV_BUILTIN(map_get) {
    lv_need(self, argc, 2, 2);
    LV m = lv_arg_map(self, argv, 0);
    return lv_map_get(m, lv_arg_str(self, argv, 1), NULL);
}

LV_BUILTIN(map_has) {
    lv_need(self, argc, 2, 2);
    LV m = lv_arg_map(self, argv, 0);
    int found;
    lv_map_get(m, lv_arg_str(self, argv, 1), &found);
    return lv_bool(found);
}

LV_BUILTIN(map_delete) {
    lv_need(self, argc, 2, 2);
    LV m = lv_arg_map(self, argv, 0);
    return lv_bool(lv_map_del(m, lv_arg_str(self, argv, 1)));
}

LV_BUILTIN(map_keys) {
    lv_need(self, argc, 1, 1);
    LV m = lv_arg_map(self, argv, 0);
    LV keys = lv_list_new(m.m->count);
    for (int64_t i = 0; i < m.m->used; i++) {
        if (m.m->entries[i].live) {
            LV k;
            k.tag = LV_STR;
            k.s = m.m->entries[i].key;
            lv_list_push(keys, k);
        }
    }
    return keys;
}

LV_BUILTIN(map_values) {
    lv_need(self, argc, 1, 1);
    LV m = lv_arg_map(self, argv, 0);
    LV out = lv_list_new(m.m->count);
    for (int64_t i = 0; i < m.m->used; i++) {
        if (m.m->entries[i].live) lv_list_push(out, m.m->entries[i].val);
    }
    return out;
}

LV_BUILTIN(map_items) {
    lv_need(self, argc, 1, 1);
    LV m = lv_arg_map(self, argv, 0);
    LV out = lv_list_new(m.m->count);
    for (int64_t i = 0; i < m.m->used; i++) {
        LvMapEntry *e = &m.m->entries[i];
        if (!e->live) continue;
        LV pair[2];
        pair[0].tag = LV_STR;
        pair[0].s = e->key;
        pair[1] = e->val;
        lv_list_push(out, lv_list_of(2, pair));
    }
    return out;
}

// ---- lists -----------------------------------------------------------------

LV_BUILTIN(remove) {
    lv_need(self, argc, 2, 2);
    LvList *l = lv_arg_list(self, argv, 0);
    int64_t i = lv_arg_int(self, argv, 1);
    int64_t orig = i;
    if (i < 0) i += l->count;
    if (i < 0 || i >= l->count) lv_oob(orig, l->count, "list");
    LV v = l->items[i];
    memmove(l->items + i, l->items + i + 1, sizeof(LV) * (size_t)(l->count - i - 1));
    l->count--;
    return v;
}

LV_BUILTIN(find) {
    lv_need(self, argc, 2, 2);
    LvList *l = lv_arg_list(self, argv, 0);
    for (int64_t i = 0; i < l->count; i++) {
        if (lv_equal(l->items[i], argv[1])) return lv_int(i);
    }
    return lv_int(-1);
}

static void lv_clamp_range(int64_t len, int64_t *a, int64_t *b) {
    if (*a < 0) *a += len;
    if (*b < 0) *b += len;
    if (*a < 0) *a = 0;
    if (*b > len) *b = len;
}

LV_BUILTIN(slice) {
    lv_need(self, argc, 3, 3);
    int64_t a = lv_arg_int(self, argv, 1), b = lv_arg_int(self, argv, 2);
    if (argv[0].tag == LV_LIST) {
        LvList *l = argv[0].l;
        lv_clamp_range(l->count, &a, &b);
        if (a >= b) return lv_list_new(0);
        return lv_list_of((int)(b - a), l->items + a);
    }
    LvStr *s = lv_arg_str(self, argv, 0);
    lv_clamp_range(s->len, &a, &b);
    if (a >= b) return lv_str("");
    return lv_str_n(s->data + a, b - a);
}

static int lv_sort_cmp(LV a, LV b) {
    if (lv_is_num(a) && lv_is_num(b)) {
        if (a.tag == LV_INT && b.tag == LV_INT) return (a.i > b.i) - (a.i < b.i);
        double x = lv_num(a), y = lv_num(b);
        return (x > y) - (x < y);
    }
    return lv_order(a, b);
}

static void lv_merge_sort(LV *items, LV *tmp, int64_t n) {
    if (n < 2) return;
    if (n <= 16) {
        for (int64_t i = 1; i < n; i++) {
            LV x = items[i];
            int64_t j = i - 1;
            while (j >= 0 && lv_sort_cmp(items[j], x) > 0) {
                items[j + 1] = items[j];
                j--;
            }
            items[j + 1] = x;
        }
        return;
    }
    int64_t h = n / 2;
    lv_merge_sort(items, tmp, h);
    lv_merge_sort(items + h, tmp, n - h);
    int64_t i = 0, j = h, k = 0;
    while (i < h && j < n) tmp[k++] = lv_sort_cmp(items[j], items[i]) < 0 ? items[j++] : items[i++];
    while (i < h) tmp[k++] = items[i++];
    while (j < n) tmp[k++] = items[j++];
    memcpy(items, tmp, sizeof(LV) * (size_t)n);
}

LV_BUILTIN(sort) {
    lv_need(self, argc, 1, 1);
    LvList *l = lv_arg_list(self, argv, 0);
    LV *tmp = lv_alloc(sizeof(LV) * (size_t)(l->count + 1));
    lv_merge_sort(l->items, tmp, l->count);
    return lv_null();
}

LV_BUILTIN(map) {
    lv_need(self, argc, 2, 2);
    LvList *l = lv_arg_list(self, argv, 0);
    LV out = lv_list_new(l->count);
    for (int64_t i = 0; i < l->count; i++) {
        LV a = l->items[i];
        lv_list_push(out, lv_call(argv[1], 1, &a));
    }
    return out;
}

LV_BUILTIN(filter) {
    lv_need(self, argc, 2, 2);
    LvList *l = lv_arg_list(self, argv, 0);
    LV out = lv_list_new(l->count);
    for (int64_t i = 0; i < l->count; i++) {
        LV a = l->items[i];
        if (lv_truth(lv_call(argv[1], 1, &a))) lv_list_push(out, a);
    }
    return out;
}

LV_BUILTIN(reduce) {
    lv_need(self, argc, 3, 3);
    LvList *l = lv_arg_list(self, argv, 0);
    LV acc = argv[2];
    for (int64_t i = 0; i < l->count; i++) {
        LV a[2] = { acc, l->items[i] };
        acc = lv_call(argv[1], 2, a);
    }
    return acc;
}

// ---- strings ---------------------------------------------------------------

LV_BUILTIN(is_empty) {
    lv_need(self, argc, 1, 1);
    return lv_bool(lv_arg_str(self, argv, 0)->len == 0);
}

LV_BUILTIN(concat) {
    lv_need(self, argc, 2, 2);
    LV a = lv_textish(argv[0]), b = lv_textish(argv[1]);
    LV args2[2] = { a, b };
    lv_arg_str(self, args2, 0);
    lv_arg_str(self, args2, 1);
    return lv_add(a, b);
}

LV_BUILTIN(substring) {
    lv_need(self, argc, 3, 3);
    LvStr *s = lv_arg_str(self, argv, 0);
    int64_t start = lv_arg_int(self, argv, 1), n = lv_arg_int(self, argv, 2);
    if (start < 0) start = 0;
    if (start >= s->len) return lv_str("");
    if (n < 0) n = 0;
    if (start + n > s->len) n = s->len - start;
    return lv_str_n(s->data + start, n);
}

LV_BUILTIN(char_at) {
    lv_need(self, argc, 2, 2);
    LvStr *s = lv_arg_str(self, argv, 0);
    int64_t i = lv_arg_int(self, argv, 1);
    if (i < 0 || i >= s->len) return lv_str("");
    return lv_str_n(s->data + i, 1);
}

static int64_t lv_find_sub(LvStr *s, LvStr *sub, int64_t from) {
    if (sub->len == 0) return from <= s->len ? from : -1;
    for (int64_t i = from; i + sub->len <= s->len; i++) {
        if (memcmp(s->data + i, sub->data, (size_t)sub->len) == 0) return i;
    }
    return -1;
}

LV_BUILTIN(index_of) {
    lv_need(self, argc, 2, 2);
    LV sub = lv_textish(argv[1]);
    LV a2[2] = { argv[0], sub };
    return lv_int(lv_find_sub(lv_arg_str(self, a2, 0), lv_arg_str(self, a2, 1), 0));
}

LV_BUILTIN(last_index_of) {
    lv_need(self, argc, 2, 2);
    LV sub = lv_textish(argv[1]);
    LV a2[2] = { argv[0], sub };
    LvStr *s = lv_arg_str(self, a2, 0), *t = lv_arg_str(self, a2, 1);
    for (int64_t i = s->len - t->len; i >= 0; i--) {
        if (memcmp(s->data + i, t->data, (size_t)t->len) == 0) return lv_int(i);
    }
    return lv_int(-1);
}

LV_BUILTIN(contains) {
    lv_need(self, argc, 2, 2);
    if (argv[0].tag == LV_LIST) {
        LvList *l = argv[0].l;
        for (int64_t i = 0; i < l->count; i++) {
            if (lv_equal(l->items[i], argv[1])) return lv_bool(1);
        }
        return lv_bool(0);
    }
    LV sub = lv_textish(argv[1]);
    LV a2[2] = { argv[0], sub };
    return lv_bool(lv_find_sub(lv_arg_str(self, a2, 0), lv_arg_str(self, a2, 1), 0) >= 0);
}

LV_BUILTIN(starts_with) {
    lv_need(self, argc, 2, 2);
    LV a2[2] = { argv[0], lv_textish(argv[1]) };
    LvStr *s = lv_arg_str(self, a2, 0), *p = lv_arg_str(self, a2, 1);
    return lv_bool(p->len <= s->len && memcmp(s->data, p->data, (size_t)p->len) == 0);
}

LV_BUILTIN(ends_with) {
    lv_need(self, argc, 2, 2);
    LV a2[2] = { argv[0], lv_textish(argv[1]) };
    LvStr *s = lv_arg_str(self, a2, 0), *p = lv_arg_str(self, a2, 1);
    return lv_bool(p->len <= s->len && memcmp(s->data + s->len - p->len, p->data, (size_t)p->len) == 0);
}

static LV lv_map_chars(LvStr *s, int (*fn)(int)) {
    LV out = lv_str_n(s->data, s->len);
    for (int64_t i = 0; i < s->len; i++) out.s->data[i] = (char)fn((unsigned char)s->data[i]);
    return out;
}

LV_BUILTIN(to_upper) {
    lv_need(self, argc, 1, 1);
    LV a = lv_textish(argv[0]);
    LV r = lv_map_chars(lv_arg_str(self, &a, 0), toupper);
    return argv[0].tag == LV_CHAR ? lv_char(r.s->data[0]) : r;
}

LV_BUILTIN(to_lower) {
    lv_need(self, argc, 1, 1);
    LV a = lv_textish(argv[0]);
    LV r = lv_map_chars(lv_arg_str(self, &a, 0), tolower);
    return argv[0].tag == LV_CHAR ? lv_char(r.s->data[0]) : r;
}

static LV lv_trim(LvStr *s, int left, int right) {
    int64_t a = 0, b = s->len;
    if (left) while (a < b && isspace((unsigned char)s->data[a])) a++;
    if (right) while (b > a && isspace((unsigned char)s->data[b - 1])) b--;
    return lv_str_n(s->data + a, b - a);
}

LV_BUILTIN(trim) { lv_need(self, argc, 1, 1); return lv_trim(lv_arg_str(self, argv, 0), 1, 1); }
LV_BUILTIN(trim_left) { lv_need(self, argc, 1, 1); return lv_trim(lv_arg_str(self, argv, 0), 1, 0); }
LV_BUILTIN(trim_right) { lv_need(self, argc, 1, 1); return lv_trim(lv_arg_str(self, argv, 0), 0, 1); }

LV_BUILTIN(replace) {
    lv_need(self, argc, 3, 3);
    LV a3[3] = { argv[0], lv_textish(argv[1]), lv_textish(argv[2]) };
    LvStr *s = lv_arg_str(self, a3, 0), *from = lv_arg_str(self, a3, 1), *to = lv_arg_str(self, a3, 2);
    if (from->len == 0) return a3[0];
    LvBuf b;
    lv_buf_init(&b);
    int64_t i = 0;
    for (;;) {
        int64_t j = lv_find_sub(s, from, i);
        if (j < 0) break;
        lv_buf_add(&b, s->data + i, j - i);
        lv_buf_add(&b, to->data, to->len);
        i = j + from->len;
    }
    lv_buf_add(&b, s->data + i, s->len - i);
    return lv_buf_done(&b);
}

LV_BUILTIN(reverse) {
    lv_need(self, argc, 1, 1);
    if (argv[0].tag == LV_LIST) {
        LvList *l = argv[0].l;
        for (int64_t i = 0, j = l->count - 1; i < j; i++, j--) {
            LV t = l->items[i];
            l->items[i] = l->items[j];
            l->items[j] = t;
        }
        return argv[0];
    }
    LvStr *s = lv_arg_str(self, argv, 0);
    LV out = lv_str_n(s->data, s->len);
    for (int64_t i = 0; i < s->len; i++) out.s->data[i] = s->data[s->len - 1 - i];
    return out;
}

LV_BUILTIN(repeat) {
    lv_need(self, argc, 2, 2);
    LV a = lv_textish(argv[0]);
    LvStr *s = lv_arg_str(self, &a, 0);
    int64_t n = lv_arg_int(self, argv, 1);
    LvBuf b;
    lv_buf_init(&b);
    for (int64_t i = 0; i < n; i++) lv_buf_add(&b, s->data, s->len);
    return lv_buf_done(&b);
}

static LV lv_pad(LvFunc *self, LV *argv, int left) {
    LvStr *s = lv_arg_str(self, argv, 0);
    int64_t w = lv_arg_int(self, argv, 1);
    LV padv = lv_textish(argv[2]);
    LvStr *pad = lv_arg_str(self, &padv, 0);
    if (s->len >= w || pad->len == 0) return argv[0];
    LvBuf b;
    lv_buf_init(&b);
    if (!left) lv_buf_add(&b, s->data, s->len);
    for (int64_t i = 0; i < w - s->len; i++) lv_buf_add(&b, pad->data, 1);
    if (left) lv_buf_add(&b, s->data, s->len);
    return lv_buf_done(&b);
}

LV_BUILTIN(pad_left) { lv_need(self, argc, 3, 3); return lv_pad(self, argv, 1); }
LV_BUILTIN(pad_right) { lv_need(self, argc, 3, 3); return lv_pad(self, argv, 0); }

LV_BUILTIN(format) {
    if (argc < 1) lv_arity(self, argc, 1, 1);
    LvStr *f = lv_arg_str(self, argv, 0);
    LvBuf b;
    lv_buf_init(&b);
    int next = 1;
    for (int64_t i = 0; i < f->len; i++) {
        if (f->data[i] == '{' && i + 1 < f->len && f->data[i + 1] == '}' && next < argc) {
            lv_buf_value(&b, argv[next++]);
            i++;
        } else {
            lv_buf_add(&b, f->data + i, 1);
        }
    }
    return lv_buf_done(&b);
}

// Splits on every occurrence of the delimiter; empty pieces are kept.
LV_BUILTIN(split) {
    lv_need(self, argc, 2, 2);
    LV a2[2] = { argv[0], lv_textish(argv[1]) };
    LvStr *s = lv_arg_str(self, a2, 0), *d = lv_arg_str(self, a2, 1);
    LV out = lv_list_new(8);
    if (d->len == 0) {
        for (int64_t i = 0; i < s->len; i++) lv_list_push(out, lv_str_n(s->data + i, 1));
        return out;
    }
    // Like the VM (strtok): every character of the delimiter separates, and
    // empty pieces are dropped.
    char *copy = lv_alloc((size_t)s->len + 1);
    memcpy(copy, s->data, (size_t)s->len + 1);
    char *save = NULL;
    for (char *tok = strtok_r(copy, d->data, &save); tok; tok = strtok_r(NULL, d->data, &save)) {
        lv_list_push(out, lv_str(tok));
    }
    return out;
}

LV_BUILTIN(join) {
    lv_need(self, argc, 2, 2);
    LvList *l = lv_arg_list(self, argv, 0);
    LV dv = lv_textish(argv[1]);
    LvStr *d = lv_arg_str(self, &dv, 0);
    LvBuf b;
    lv_buf_init(&b);
    for (int64_t i = 0; i < l->count; i++) {
        if (i) lv_buf_add(&b, d->data, d->len);
        lv_buf_value(&b, l->items[i]);
    }
    return lv_buf_done(&b);
}

static LV lv_char_class(LvFunc *self, int argc, LV *argv, int (*fn)(int)) {
    lv_need(self, argc, 1, 1);
    LV a = lv_textish(argv[0]);
    LvStr *s = lv_arg_str(self, &a, 0);
    if (s->len == 0) return lv_bool(0);
    for (int64_t i = 0; i < s->len; i++) {
        if (!fn((unsigned char)s->data[i])) return lv_bool(0);
    }
    return lv_bool(1);
}

LV_BUILTIN(is_digit) { return lv_char_class(self, argc, argv, isdigit); }
LV_BUILTIN(is_alpha) { return lv_char_class(self, argc, argv, isalpha); }
LV_BUILTIN(is_alnum) { return lv_char_class(self, argc, argv, isalnum); }
LV_BUILTIN(is_space) { return lv_char_class(self, argc, argv, isspace); }

// ---- math ------------------------------------------------------------------

LV_BUILTIN(abs) {
    lv_need(self, argc, 1, 1);
    if (argv[0].tag == LV_INT) {
        if (argv[0].i == INT64_MIN) lv_overflow("abs");
        return lv_int(argv[0].i < 0 ? -argv[0].i : argv[0].i);
    }
    return lv_float(fabs(lv_arg_num(self, argv, 0)));
}

LV_BUILTIN(min) {
    lv_need(self, argc, 2, 2);
    lv_arg_num(self, argv, 0);
    lv_arg_num(self, argv, 1);
    return lv_sort_cmp(argv[1], argv[0]) < 0 ? argv[1] : argv[0];
}

LV_BUILTIN(max) {
    lv_need(self, argc, 2, 2);
    lv_arg_num(self, argv, 0);
    lv_arg_num(self, argv, 1);
    return lv_sort_cmp(argv[1], argv[0]) > 0 ? argv[1] : argv[0];
}

LV_BUILTIN(clamp) {
    lv_need(self, argc, 3, 3);
    lv_arg_num(self, argv, 0);
    lv_arg_num(self, argv, 1);
    lv_arg_num(self, argv, 2);
    if (lv_sort_cmp(argv[0], argv[1]) < 0) return argv[1];
    if (lv_sort_cmp(argv[0], argv[2]) > 0) return argv[2];
    return argv[0];
}

LV_BUILTIN(sign) {
    lv_need(self, argc, 1, 1);
    double x = lv_arg_num(self, argv, 0);
    return lv_int((x > 0) - (x < 0));
}

#define LV_MATH1(name, expr)                         \
    LV_BUILTIN(name) {                               \
        lv_need(self, argc, 1, 1);                   \
        double x = lv_arg_num(self, argv, 0);        \
        return lv_float(expr);                       \
    }

LV_MATH1(sqrt, sqrt(x))
// glibc returns 3.0000000000000004 for cbrt(27); make exact cubes exact.
static double lv_cbrt(double x) {
    double r = cbrt(x);
    double n = round(r);
    return n * n * n == x ? n : r;
}
LV_MATH1(cbrt, lv_cbrt(x))
LV_MATH1(exp, exp(x))
LV_MATH1(ln, log(x))
LV_MATH1(log10, log10(x))
LV_MATH1(sin, sin(x))
LV_MATH1(cos, cos(x))
LV_MATH1(tan, tan(x))
LV_MATH1(asin, asin(x))
LV_MATH1(acos, acos(x))
LV_MATH1(atan, atan(x))
LV_MATH1(sinh, sinh(x))
LV_MATH1(cosh, cosh(x))
LV_MATH1(tanh, tanh(x))
LV_MATH1(fract, x - floor(x))
LV_MATH1(deg_to_rad, x * 3.14159265358979323846 / 180.0)
LV_MATH1(rad_to_deg, x * 180.0 / 3.14159265358979323846)

static LV lv_round_to_int(double r) {
    if (!(r > -9.3e18 && r < 9.3e18)) return lv_float(r);
    return lv_int((int64_t)r);
}

#define LV_ROUND(name, fn)                                        \
    LV_BUILTIN(name) {                                            \
        lv_need(self, argc, 1, 1);                                \
        if (argv[0].tag == LV_INT) return argv[0];                \
        return lv_round_to_int(fn(lv_arg_num(self, argv, 0)));    \
    }

LV_ROUND(floor, floor)
LV_ROUND(ceil, ceil)
LV_ROUND(round, round)
LV_ROUND(trunc, trunc)

LV_BUILTIN(pow) {
    lv_need(self, argc, 2, 2);
    return lv_float(pow(lv_arg_num(self, argv, 0), lv_arg_num(self, argv, 1)));
}

LV_BUILTIN(atan2) {
    lv_need(self, argc, 2, 2);
    return lv_float(atan2(lv_arg_num(self, argv, 0), lv_arg_num(self, argv, 1)));
}

LV_BUILTIN(mod) {
    lv_need(self, argc, 2, 2);
    double b = lv_arg_num(self, argv, 1);
    if (b == 0.0) lv_div_zero();
    return lv_float(fmod(lv_arg_num(self, argv, 0), b));
}

LV_BUILTIN(lerp) {
    lv_need(self, argc, 3, 3);
    double a = lv_arg_num(self, argv, 0), b = lv_arg_num(self, argv, 1), t = lv_arg_num(self, argv, 2);
    return lv_float(a + (b - a) * t);
}

// ---- files -----------------------------------------------------------------

static LvFile *lv_arg_file(LvFunc *self, LV *argv, int i) {
    if (argv[i].tag != LV_FILE) {
        lv_errorf(LV_ERR_TYPE, NULL, "%s() argument %d must be a file, got %s",
                  lv_fname(self), i + 1, lv_type_name(argv[i]));
    }
    LvFile *f = argv[i].file;
    if (f->closed) lv_errorf(LV_ERR_RUNTIME, NULL, "%s() on a closed file", lv_fname(self));
    return f;
}

LV_BUILTIN(open) {
    lv_need(self, argc, 2, 2);
    LvStr *path = lv_arg_str(self, argv, 0), *mode = lv_arg_str(self, argv, 1);
    if (strcmp(mode->data, "r") && strcmp(mode->data, "w") && strcmp(mode->data, "a")) {
        lv_errorf(LV_ERR_ARGUMENT, "Use \"r\", \"w\" or \"a\"", "open() mode '%s' is not supported", mode->data);
    }
    FILE *fp = fopen(path->data, mode->data);
    if (!fp) return lv_null();
    LvFile *f = lv_alloc(sizeof(LvFile));
    f->fp = fp;
    f->closed = 0;
    LV v;
    v.tag = LV_FILE;
    v.file = f;
    return v;
}

LV_BUILTIN(close) {
    lv_need(self, argc, 1, 1);
    LvFile *f = lv_arg_file(self, argv, 0);
    fclose(f->fp);
    f->closed = 1;
    return lv_null();
}

LV_BUILTIN(read) {
    lv_need(self, argc, 1, 1);
    LvFile *f = lv_arg_file(self, argv, 0);
    LvBuf b;
    lv_buf_init(&b);
    char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f->fp)) > 0) lv_buf_add(&b, chunk, (int64_t)n);
    return lv_buf_done(&b);
}

LV_BUILTIN(read_line) {
    lv_need(self, argc, 1, 1);
    LvFile *f = lv_arg_file(self, argv, 0);
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, f->fp);
    if (n < 0) {
        free(line);
        return lv_null();
    }
    if (n > 0 && line[n - 1] == '\n') n--;
    LV r = lv_str_n(line, n);
    free(line);
    return r;
}

// ---- dense lists and vector math (ported from src/vec_lib.c, src/list_lib.c) ----

LV_BUILTIN(dense_list) {
    if (argc != 2 || argv[0].tag != LV_INT || argv[1].tag != LV_FLOAT) {
        lv_error(LV_ERR_ARGUMENT, "dense_list() expects (size: int, fill: float)", "Usage: dense_list(100, 1.5)");
    }
    int64_t n = argv[0].i;
    if (n <= 0) return lv_dense_new(0);
    LV r = lv_dense_new(n);
    for (int64_t i = 0; i < n; i++) r.d->data[i] = argv[1].f;
    return r;
}

static LV lv_vec_op(LvFunc *self, int argc, LV *argv, int op) {
    if (argc != 2) {
        printf("Error: %s expects 2 lists\n", lv_fname(self));
        return lv_null();
    }
    return lv_list_arith(op, argv[0], argv[1]);
}

LV_BUILTIN(vec_add) { return lv_vec_op(self, argc, argv, 0); }
LV_BUILTIN(vec_sub) { return lv_vec_op(self, argc, argv, 1); }
LV_BUILTIN(vec_mul) { return lv_vec_op(self, argc, argv, 2); }
LV_BUILTIN(vec_div) { return lv_vec_op(self, argc, argv, 3); }

LV_BUILTIN(vec_mul_inline) {
    if (argc != 2) {
        printf("Error: vec_mul_inline expects 2 lists\n");
        return lv_null();
    }
    if (argv[0].tag == LV_DENSE && argv[1].tag == LV_DENSE) {
        int64_t n = argv[0].d->count < argv[1].d->count ? argv[0].d->count : argv[1].d->count;
        for (int64_t i = 0; i < n; i++) argv[0].d->data[i] *= argv[1].d->data[i];
    } else {
        printf("Error: vec_mul_inline requires dense lists for A and B.\n");
    }
    return lv_null();
}

static int64_t lv_row_len(LV r) {
    return r.tag == LV_LIST ? r.l->count : r.tag == LV_DENSE ? r.d->count : 0;
}

static double lv_row_at(LV r, int64_t j) {
    return r.tag == LV_DENSE ? r.d->data[j] : lv_elem(r.l->items[j]);
}

LV_BUILTIN(mat_mul) {
    if (argc != 2) return lv_null();
    LV A = argv[0], B = argv[1];
    if (!lv_is_listish(A) || !lv_is_listish(B)) return lv_null();
    int64_t rows_a = A.tag == LV_LIST ? A.l->count : 1;
    int64_t rows_b = B.tag == LV_LIST ? B.l->count : 1;
    if (rows_a == 0 || rows_b == 0) return lv_list_new(0);
    LV first_a = (A.tag == LV_LIST && A.l->count > 0) ? A.l->items[0] : A;
    LV first_b = (B.tag == LV_LIST && B.l->count > 0) ? B.l->items[0] : B;
    int64_t cols_a = lv_row_len(first_a), cols_b = lv_row_len(first_b);
    if (cols_a != rows_b || cols_a == 0) {
        printf("Runtime Error: Matrix dimension mismatch (%d cols vs %d rows)\n", (int)cols_a, (int)rows_b);
        return lv_null();
    }
    double *fa = lv_alloc(sizeof(double) * (size_t)(rows_a * cols_a));
    double *fb = lv_alloc(sizeof(double) * (size_t)(rows_b * cols_b));
    double *fc = calloc((size_t)(rows_a * cols_b), sizeof(double));
    if (!fc) lv_error(LV_ERR_RUNTIME, "out of memory", NULL);
    for (int64_t i = 0; i < rows_a; i++) {
        LV row = A.tag == LV_LIST ? A.l->items[i] : A;
        for (int64_t j = 0; j < cols_a; j++) fa[i * cols_a + j] = lv_row_at(row, j);
    }
    for (int64_t i = 0; i < rows_b; i++) {
        LV row = B.tag == LV_LIST ? B.l->items[i] : B;
        for (int64_t j = 0; j < cols_b; j++) fb[i * cols_b + j] = lv_row_at(row, j);
    }
    for (int64_t i = 0; i < rows_a; i++)
        for (int64_t k = 0; k < cols_a; k++) {
            double av = fa[i * cols_a + k];
            for (int64_t j = 0; j < cols_b; j++) fc[i * cols_b + j] += av * fb[k * cols_b + j];
        }
    LV res = lv_list_new(rows_a);
    for (int64_t i = 0; i < rows_a; i++) {
        LV row = lv_dense_new(cols_b);
        for (int64_t j = 0; j < cols_b; j++) row.d->data[j] = fc[i * cols_b + j];
        lv_list_push(res, row);
    }
    free(fc);
    return res;
}

// xoroshiro128++, seeded and scrambled exactly as in src/math_lib.c.
static uint64_t lv_rng[2] = {0x12345678, 0x87654321};

static inline uint64_t lv_rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static uint64_t lv_rng_next(void) {
    const uint64_t s0 = lv_rng[0];
    uint64_t s1 = lv_rng[1];
    const uint64_t result = lv_rotl(s0 + s1, 17) + s0;
    s1 ^= s0;
    lv_rng[0] = lv_rotl(s0, 49) ^ s1 ^ (s1 << 21);
    lv_rng[1] = lv_rotl(s1, 28);
    return result;
}

static uint64_t lv_os_entropy(void) {
    uint64_t val = 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(&val, sizeof(val), 1, f) != 1) val = (uint64_t)time(NULL);
        fclose(f);
    } else {
        val = (uint64_t)time(NULL);
    }
    return val;
}

LV_BUILTIN(trand) { return lv_int((int64_t)lv_os_entropy()); }

LV_BUILTIN(srand) {
    uint64_t seed;
    if (argc == 0) seed = lv_os_entropy();
    else seed = argv[0].tag == LV_INT ? (uint64_t)argv[0].i : (uint64_t)argv[0].f;
    uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    lv_rng[0] = z ^ (z >> 31);
    z = lv_rng[0] + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    lv_rng[1] = z ^ (z >> 31);
    return lv_null();
}

LV_BUILTIN(rand) {
    if (argc == 0) {
        uint64_t r = lv_rng_next();
        return lv_float((double)(r >> 11) * (1.0 / 9007199254740992.0));
    }
    int64_t lo = 0, hi = 0;
    if (argc == 1) {
        hi = argv[0].tag == LV_INT ? argv[0].i : (int64_t)argv[0].f;
    } else if (argc == 2) {
        lo = argv[0].tag == LV_INT ? argv[0].i : (int64_t)argv[0].f;
        hi = argv[1].tag == LV_INT ? argv[1].i : (int64_t)argv[1].f;
    } else {
        fprintf(stderr, "Runtime Error: rand() takes 0, 1, or 2 arguments.\n");
        return lv_null();
    }
    if (lo > hi) { int64_t t = lo; lo = hi; hi = t; }
    uint64_t range = (uint64_t)(hi - lo + 1);
    if (range == 0) return lv_int(lo);
    return lv_int(lo + (int64_t)(lv_rng_next() % range));
}

LV_BUILTIN(shuffle) {
    if (argc != 1 || argv[0].tag != LV_LIST) {
        lv_error(LV_ERR_ARGUMENT, "shuffle() expects 1 list", "Usage: shuffle(myList)");
    }
    LvList *l = argv[0].l;
    int64_t n = l->count;
    for (int64_t i = n - 1; i > 0; i--) {
        int64_t j = (int64_t)(lv_rng_next() % (uint64_t)(i + 1));
        LV t = l->items[i];
        l->items[i] = l->items[j];
        l->items[j] = t;
    }
    return lv_null();
}

// Stalin sort: drops every element smaller than the last one kept.
LV_BUILTIN(ssort) {
    if (argc != 1 || (argv[0].tag != LV_LIST && argv[0].tag != LV_DENSE)) {
        lv_error(LV_ERR_ARGUMENT, "ssort() expects 1 list", "Usage: ssort(myList)");
    }
    if (argv[0].tag == LV_LIST) {
        LvList *l = argv[0].l;
        if (l->count <= 1) return lv_null();
        int64_t w = 1;
        LV last = l->items[0];
        for (int64_t r = 1; r < l->count; r++) {
            LV cur = l->items[r];
            if (lv_sort_cmp(cur, last) >= 0) {
                l->items[w++] = cur;
                last = cur;
            }
        }
        l->count = w;
    } else {
        LvDense *d = argv[0].d;
        if (d->count <= 1) return lv_null();
        int64_t w = 1;
        double last = d->data[0];
        for (int64_t r = 1; r < d->count; r++) {
            if (d->data[r] >= last) {
                d->data[w++] = d->data[r];
                last = d->data[r];
            }
        }
        d->count = w;
    }
    return lv_null();
}


LV_BUILTIN(write) {
    lv_need(self, argc, 2, 2);
    LvFile *f = lv_arg_file(self, argv, 0);
    LV s = lv_to_string(argv[1]);
    size_t w = fwrite(s.s->data, 1, (size_t)s.s->len, f->fp);
    return lv_bool(w == (size_t)s.s->len);
}

LV_BUILTIN(file_exists) {
    lv_need(self, argc, 1, 1);
    return lv_bool(access(lv_arg_str(self, argv, 0)->data, F_OK) == 0);
}

LV_BUILTIN(remove_file) {
    lv_need(self, argc, 1, 1);
    return lv_bool(remove(lv_arg_str(self, argv, 0)->data) == 0);
}

LV_BUILTIN(flush) {
    lv_need(self, argc, 1, 1);
    fflush(lv_arg_file(self, argv, 0)->fp);
    return lv_null();
}
