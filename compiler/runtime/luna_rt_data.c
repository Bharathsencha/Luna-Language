// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// Structured data and manual memory for code emitted by lunac: data, template,
// bloc, box, and the unsafe-block pointer rules. The behaviour and the error
// text follow src/value.c, src/unsafe_runtime.c and src/luna_vm.c. Validation
// is done by the same Rust libraries the VM links (libluna_data_rt.a and
// libluna_memory_rt.a).

#define _GNU_SOURCE
#include "luna_rt.h"

#include <stdlib.h>
#include <string.h>

// ---- Rust FFI (rust/data_rt, rust/unsafe_rt) -------------------------------

int luna_data_bloc_validate(const char *name, const char **fields, int count);
int luna_data_box_size_ok(size_t size);
void luna_data_box_track(uint64_t handle);
int luna_data_box_access_ok(uint64_t handle);
int luna_data_box_free_ok(uint64_t handle);
int luna_data_template_register(const char *name, const char **fields, int count);
void luna_data_error_message(int code, char *buf, size_t buf_len);

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
int luna_mem_gc_store_ok(uintptr_t ptr);
void luna_mem_error_message(int code, char *buf, size_t buf_len);

// ---- blocs -----------------------------------------------------------------

#define BLOC_MAX_BYTES 32
#define BLOC_SLOT_BYTES 8

enum { BK_UNSET = 0, BK_INT, BK_FLOAT, BK_BOOL, BK_CHAR, BK_BLOC, BK_PRIM };

struct LvBlocDesc {
    const char *name;
    int nfields;
    const char **fields;
    unsigned char *kinds;
    LvBlocDesc **nested;
    int total_size;
    int locked;
    LvBlocDesc *next;
};

static LvBlocDesc *bloc_registry = NULL;

static LV lv_wrap(LvTag tag) {
    LV v;
    v.tag = tag;
    v.i = 0;
    return v;
}

static int bloc_kind_of(LV v) {
    switch (v.tag) {
        case LV_INT: return BK_INT;
        case LV_FLOAT: return BK_FLOAT;
        case LV_BOOL: return BK_BOOL;
        case LV_CHAR: return BK_CHAR;
        case LV_BLOC: return BK_BLOC;
        default: return BK_UNSET;
    }
}

static LvBlocDesc *bloc_find(const char *name) {
    for (LvBlocDesc *d = bloc_registry; d; d = d->next) {
        if (strcmp(d->name, name) == 0) return d;
    }
    return NULL;
}

static int bloc_same_schema(const LvBlocDesc *d, const char *const *fields, int n) {
    if (d->nfields != n) return 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(d->fields[i], fields[i]) != 0) return 0;
    }
    return 1;
}

LV lv_define_bloc(const char *name, int n, const char *const *fields) {
    char msg[256];
    int code = luna_data_bloc_validate(name, (const char **)fields, n);
    if (code != 0) {
        luna_data_error_message(code, msg, sizeof(msg));
        lv_error(LV_ERR_TYPE, msg, "A bloc holds at most 32 bytes of unique fields; each name keeps one field list");
    }
    LvBlocDesc *existing = bloc_find(name);
    if (existing) {
        if (!bloc_same_schema(existing, fields, n)) {
            snprintf(msg, sizeof(msg), "bloc '%s' is already defined with different fields", name);
            lv_error(LV_ERR_TYPE, msg, "A bloc holds at most 32 bytes of unique fields; each name keeps one field list");
        }
        LV v = lv_wrap(LV_BLOC_TYPE);
        v.bd = existing;
        return v;
    }
    LvBlocDesc *d = lv_alloc(sizeof(LvBlocDesc));
    d->name = name;
    d->nfields = n;
    d->fields = n ? lv_alloc(sizeof(char *) * (size_t)n) : NULL;
    d->kinds = n ? lv_alloc_atomic((size_t)n) : NULL;
    d->nested = n ? lv_alloc(sizeof(LvBlocDesc *) * (size_t)n) : NULL;
    for (int i = 0; i < n; i++) d->fields[i] = fields[i];
    d->next = bloc_registry;
    bloc_registry = d;
    LV v = lv_wrap(LV_BLOC_TYPE);
    v.bd = d;
    return v;
}

static int bloc_validate(LvBlocDesc *d, int argc, LV *argv, char *msg, size_t msg_len) {
    if (argc != d->nfields) {
        snprintf(msg, msg_len, "Bloc '%s' expects %d field value(s), but got %d", d->name, d->nfields, argc);
        return 0;
    }
    int offset = 0;
    for (int i = 0; i < argc; i++) {
        int kind = bloc_kind_of(argv[i]);
        if (kind == BK_UNSET) {
            snprintf(msg, msg_len, "Bloc field '%s' only accepts int, float, bool, char, or another bloc", d->fields[i]);
            return 0;
        }
        int size = BLOC_SLOT_BYTES;
        LvBlocDesc *nested = NULL;
        if (kind == BK_BLOC) {
            nested = argv[i].bloc->desc;
            size = nested ? nested->total_size : 0;
        }
        if (size <= 0) {
            snprintf(msg, msg_len, "Bloc field '%s' has an invalid inline size", d->fields[i]);
            return 0;
        }
        int slot_class = kind == BK_BLOC ? BK_BLOC : BK_PRIM;
        if (d->locked) {
            if (d->kinds[i] != slot_class) {
                snprintf(msg, msg_len,
                         d->kinds[i] == BK_BLOC ? "Bloc '%s' field '%s' must hold a nested bloc"
                                                : "Bloc '%s' field '%s' must hold an int, float, bool or char",
                         d->name, d->fields[i]);
                return 0;
            }
            if (slot_class == BK_BLOC && d->nested[i] != nested) {
                snprintf(msg, msg_len, "Bloc '%s' field '%s' must use the same nested bloc type", d->name, d->fields[i]);
                return 0;
            }
            continue;
        }
        d->kinds[i] = (unsigned char)slot_class;
        d->nested[i] = nested;
        offset += size;
    }
    if (!d->locked) {
        if (offset > BLOC_MAX_BYTES) {
            snprintf(msg, msg_len, "Bloc '%s' exceeds the %d-byte cache-line cap", d->name, BLOC_MAX_BYTES);
            return 0;
        }
        d->total_size = offset;
        d->locked = 1;
    }
    return 1;
}

static LV bloc_construct(LV type, int argc, LV *argv) {
    char msg[256];
    if (lv_unsafe_depth() > 0) {
        lv_error(LV_ERR_TYPE, "box and bloc values are not allowed inside unsafe blocks",
                 "Bloc values must stay inline and only use int, float, bool, char, or nested blocs");
    }
    LvBlocDesc *d = type.bd;
    if (!bloc_validate(d, argc, argv, msg, sizeof(msg))) {
        lv_error(LV_ERR_TYPE, msg, "Bloc values must stay inline and only use int, float, bool, char, or nested blocs");
    }
    LvBloc *b = lv_alloc(sizeof(LvBloc) + sizeof(LV) * (size_t)(argc ? argc : 1));
    b->desc = d;
    for (int i = 0; i < argc; i++) b->vals[i] = argv[i];
    LV v = lv_wrap(LV_BLOC);
    v.bloc = b;
    return v;
}

int lv_bloc_equal(LV a, LV b) {
    if (a.bloc == b.bloc) return 1;
    if (a.bloc->desc != b.bloc->desc) return 0;
    for (int i = 0; i < a.bloc->desc->nfields; i++) {
        LV x = a.bloc->vals[i], y = b.bloc->vals[i];
        if (x.tag != y.tag) return 0;
        switch (x.tag) {
            case LV_INT: if (x.i != y.i) return 0; break;
            case LV_FLOAT: if (memcmp(&x.f, &y.f, sizeof(double)) != 0) return 0; break;
            case LV_BOOL: if (x.b != y.b) return 0; break;
            case LV_CHAR: if (x.c != y.c) return 0; break;
            case LV_BLOC: if (!lv_bloc_equal(x, y)) return 0; break;
            default: return 0;
        }
    }
    return 1;
}

static const char *bloc_name(LV v) { return v.bloc->desc ? v.bloc->desc->name : "bloc"; }

// ---- data and template types -----------------------------------------------

struct LvDType {
    const char *name;
    int nfields;
    const char **fields;
    int is_template;
};

typedef struct DataSchema {
    const char *name;
    const char **fields;
    int nfields;
    int is_template;
    struct DataSchema *next;
} DataSchema;

static DataSchema *schema_registry = NULL;

LV lv_define_data(const char *name, int n, const char *const *fields, int is_template) {
    char msg[256];
    const char *hint = "Each data or template name must keep one field list";
    if (is_template) {
        int code = luna_data_template_register(name, (const char **)fields, n);
        if (code != 0) {
            luna_data_error_message(code, msg, sizeof(msg));
            lv_error(LV_ERR_TYPE, msg, hint);
        }
    }
    int found = 0;
    for (DataSchema *d = schema_registry; d; d = d->next) {
        if (strcmp(d->name, name) != 0) continue;
        int same = d->nfields == n && d->is_template == is_template;
        for (int i = 0; same && i < n; i++) {
            if (strcmp(d->fields[i], fields[i]) != 0) same = 0;
        }
        if (!same) {
            snprintf(msg, sizeof(msg), "%s '%s' is already defined with different fields",
                     is_template ? "template" : "data type", name);
            lv_error(LV_ERR_TYPE, msg, hint);
        }
        found = 1;
        break;
    }
    if (!found) {
        DataSchema *d = lv_alloc(sizeof(DataSchema));
        d->name = name;
        d->nfields = n;
        d->is_template = is_template;
        d->fields = n ? lv_alloc(sizeof(char *) * (size_t)n) : NULL;
        for (int i = 0; i < n; i++) d->fields[i] = fields[i];
        d->next = schema_registry;
        schema_registry = d;
    }
    LvDType *t = lv_alloc(sizeof(LvDType));
    t->name = name;
    t->nfields = n;
    t->fields = n ? lv_alloc(sizeof(char *) * (size_t)n) : NULL;
    for (int i = 0; i < n; i++) t->fields[i] = fields[i];
    t->is_template = is_template;
    LV v = lv_wrap(LV_DTYPE);
    v.dt = t;
    return v;
}

struct LvTemplate {
    LvDType *dt;
    int n;
    LV fields[];
};

static void box_promote(LV box);
static void box_promote_owned(LV box, LvTemplate *owner);

static LV data_construct(LV type, int argc, LV *argv) {
    LvDType *t = type.dt;
    if (argc != t->nfields) {
        lv_errorf(LV_ERR_ARGUMENT, "Pass one value for each declared data field",
                  "Data constructor '%s' expects %d field value(s), but got %d", t->name, t->nfields, argc);
    }
    if (t->is_template) {
        LvTemplate *tp = lv_alloc(sizeof(LvTemplate) + sizeof(LV) * (size_t)(argc ? argc : 1));
        tp->dt = t;
        tp->n = argc;
        for (int i = 0; i < argc; i++) {
            tp->fields[i] = argv[i];
            if (argv[i].tag == LV_BOX) box_promote_owned(argv[i], tp);
        }
        LV v = lv_wrap(LV_TEMPLATE);
        v.tpl = tp;
        return v;
    }
    LV m = lv_map_new();
    lv_map_put(m, lv_str("__tag").s, lv_str(t->name));
    for (int i = 0; i < t->nfields; i++) lv_map_put(m, lv_str(t->fields[i]).s, argv[i]);
    return m;
}

LV lv_construct(LV type, int argc, LV *argv) {
    if (type.tag == LV_BLOC_TYPE) return bloc_construct(type, argc, argv);
    return data_construct(type, argc, argv);
}

static int tpl_find(const LvTemplate *t, const char *field) {
    for (int i = 0; i < t->dt->nfields; i++) {
        if (strcmp(t->dt->fields[i], field) == 0) return i;
    }
    return -1;
}

static void missing_field(LV target, const char *field) {
    if (target.tag == LV_TEMPLATE) {
        lv_errorf(LV_ERR_NAME, "Access only declared fields for this template",
                  "Template field '%s' does not exist", field);
    }
    lv_errorf(LV_ERR_NAME, "Use dot access with a declared bloc field such as point.x",
              "Field '%s' does not exist on this bloc value", field);
}

// ---- boxes -----------------------------------------------------------------

#define BOX_SLOT_MAX 4096
#define BOX_MAX_BYTES 4096

typedef struct {
    int in_use;
    uint32_t gen;
    int owned_by_template;
    uint64_t scope_id;
    size_t len;
    unsigned char *bytes;
} BoxSlot;

static BoxSlot box_slots[BOX_SLOT_MAX];
static uint64_t box_high_water = 0;
static uint64_t scope_counter = 0;

static uint64_t box_handle_make(uint64_t index, uint32_t gen) { return ((uint64_t)gen << 32) | (index + 1); }

static BoxSlot *box_slot(uint64_t handle) {
    uint64_t index = handle & 0xffffffffULL;
    if (index == 0 || index > BOX_SLOT_MAX) return NULL;
    BoxSlot *s = &box_slots[index - 1];
    if (!s->in_use || s->gen != (uint32_t)(handle >> 32)) return NULL;
    return s;
}

static void box_free_slot(BoxSlot *s) {
    if (!s || !s->in_use) return;
    (void)luna_data_box_free_ok(box_handle_make((uint64_t)(s - box_slots), s->gen));
    free(s->bytes);
    s->bytes = NULL;
    s->len = 0;
    s->in_use = 0;
    s->gen++;
    s->owned_by_template = 0;
    s->scope_id = 0;
}

uint64_t lv_scope_new(void) { return ++scope_counter; }

LV lv_box_new(LV size, uint64_t scope_id) {
    if (size.tag != LV_INT) {
        lv_error(LV_ERR_TYPE, "box[...] expects an integer byte size", "Use box[128] or another positive integer size");
    }
    const char *hint = "Use box[n] with n between 1 and 4096; at most 4096 boxes can be live at once";
    if (lv_unsafe_depth() > 0) {
        lv_error(LV_ERR_ARGUMENT, "box and bloc values are not allowed inside unsafe blocks", hint);
    }
    if (size.i < 0 || luna_data_box_size_ok((size_t)size.i) != 0) {
        lv_error(LV_ERR_ARGUMENT, "box size must be between 1 and 4096 bytes", hint);
    }
    for (uint64_t i = 0; i < BOX_SLOT_MAX; i++) {
        BoxSlot *s = &box_slots[i];
        if (s->in_use) continue;
        s->bytes = calloc((size_t)size.i, 1);
        if (!s->bytes) lv_error(LV_ERR_RUNTIME, "out of memory", NULL);
        s->in_use = 1;
        s->owned_by_template = 0;
        s->scope_id = scope_id;
        s->len = (size_t)size.i;
        if (i + 1 > box_high_water) box_high_water = i + 1;
        uint64_t handle = box_handle_make(i, s->gen);
        luna_data_box_track(handle);
        LV v = lv_wrap(LV_BOX);
        v.box = handle;
        return v;
    }
    lv_error(LV_ERR_ARGUMENT, "box allocation failed: more than 4096 boxes are live at once", hint);
}

void lv_box_release_scope(uint64_t scope_id) {
    if (!scope_id) return;
    for (uint64_t i = 0; i < box_high_water && i < BOX_SLOT_MAX; i++) {
        BoxSlot *s = &box_slots[i];
        if (s->in_use && !s->owned_by_template && s->scope_id == scope_id) box_free_slot(s);
    }
}

static void box_promote(LV box) {
    BoxSlot *s = box_slot(box.box);
    if (!s) return;
    s->owned_by_template = 1;
    s->scope_id = 0;
}

// Weak table of (template, box): when a template is collected, its boxes go too.
typedef struct {
    const void *owner;
    uint64_t handle;
} BoxOwner;

static BoxOwner *owners = NULL;
static size_t owner_count = 0, owner_cap = 0;

static void box_promote_owned(LV box, LvTemplate *owner) {
    box_promote(box);
    if (owner_count == owner_cap) {
        owner_cap = owner_cap ? owner_cap * 2 : 64;
        owners = realloc(owners, sizeof(BoxOwner) * owner_cap);
        if (!owners) lv_error(LV_ERR_RUNTIME, "out of memory", NULL);
    }
    owners[owner_count].owner = owner;
    owners[owner_count].handle = box.box;
    owner_count++;
}

// Runs after marking: boxes whose template is unreachable are freed.
void lv_data_gc_hook(void) {
    size_t keep = 0;
    for (size_t i = 0; i < owner_count; i++) {
        if (lv_gc_is_marked(owners[i].owner)) {
            owners[keep++] = owners[i];
        } else {
            BoxSlot *s = box_slot(owners[i].handle);
            if (s) box_free_slot(s);
        }
    }
    owner_count = keep;
}

// A box is freed when its scope ends, so it may not outlive it.
void lv_box_escape_check(LV v, const char *where) {
    if (v.tag != LV_BOX) return;
    BoxSlot *s = box_slot(v.box);
    if (s && s->owned_by_template) return;
    lv_errorf(LV_ERR_RUNTIME, "Keep the box in a local variable, or store it in a template field.",
              "a box cannot be %s; it is freed when its scope ends", where);
}

static void box_live(LV box) {
    if (box_slot(box.box)) return;
    int code = luna_data_box_access_ok(box.box);
    lv_error(LV_ERR_RUNTIME, code == 5 ? "box was already freed (use-after-free)" : "expected a live box value",
             "Do not use a box after it is freed.");
}

static BoxSlot *box_byte_slot(LV box, LV index, long long *i, const char *type_hint, const char *hint) {
    if (index.tag != LV_INT) lv_error(LV_ERR_TYPE, "box index must be an integer", type_hint);
    BoxSlot *s = box_slot(box.box);
    if (!s) {
        int code = luna_data_box_access_ok(box.box);
        lv_error(LV_ERR_RUNTIME, code == 5 ? "box was already freed (use-after-free)" : "expected a live box value", hint);
    }
    long long idx = index.i;
    long long len = (long long)s->len;
    if (idx < 0) idx += len;
    if (idx < 0 || idx >= len) {
        lv_errorf(LV_ERR_RUNTIME, hint, "box index %lld is out of bounds for a box of %lld bytes", idx, len);
    }
    *i = idx;
    return s;
}

// ---- operations hooked into the core runtime -------------------------------

// Reads target[index] for a template, bloc or box. Returns 1 when handled.
int lv_data_index(LV t, LV idx, LV *out) {
    if (t.tag == LV_TEMPLATE && idx.tag == LV_STR) {
        int i = tpl_find(t.tpl, idx.s->data);
        if (i < 0) missing_field(t, idx.s->data);
        *out = t.tpl->fields[i];
        return 1;
    }
    if (t.tag == LV_BLOC && idx.tag == LV_STR) {
        for (int i = 0; i < t.bloc->desc->nfields; i++) {
            if (strcmp(t.bloc->desc->fields[i], idx.s->data) == 0) {
                *out = t.bloc->vals[i];
                return 1;
            }
        }
        missing_field(t, idx.s->data);
    }
    if (t.tag == LV_BOX) {
        long long i;
        BoxSlot *s = box_byte_slot(t, idx, &i, "Use b[i] with 0 <= i < b.len", "Use b[i] with 0 <= i < b.len");
        *out = lv_int(s->bytes[i]);
        return 1;
    }
    return 0;
}

// Reads target.field for a template, bloc or box. Returns 1 when handled.
int lv_data_field(LV t, LV key, LV *out) {
    if (t.tag == LV_TEMPLATE || t.tag == LV_BLOC) return lv_data_index(t, key, out);
    if (t.tag == LV_BOX) {
        box_live(t);
        BoxSlot *s = box_slot(t.box);
        if (strcmp(key.s->data, "len") == 0 || strcmp(key.s->data, "cap") == 0) {
            *out = lv_int((int64_t)s->len);
            return 1;
        }
        lv_errorf(LV_ERR_NAME, "Use box.len or box.cap for phase-1 box values",
                  "Field '%s' does not exist on this box value", key.s->data);
    }
    return 0;
}

// Writes target[index] = val for a template, bloc or box. Returns 1 when handled.
int lv_data_index_set(LV t, LV idx, LV val) {
    if (t.tag == LV_BOX) {
        long long i;
        BoxSlot *s = box_byte_slot(t, idx, &i, "Use b[i] = byte with 0 <= i < b.len",
                                   "Use b[i] = byte with 0 <= i < b.len and 0 <= byte <= 255");
        if (val.tag != LV_INT || val.i < 0 || val.i > 255) {
            lv_error(LV_ERR_RUNTIME, "box bytes must be integers from 0 to 255",
                     "Use b[i] = byte with 0 <= i < b.len and 0 <= byte <= 255");
        }
        s->bytes[i] = (unsigned char)val.i;
        return 1;
    }
    if (t.tag == LV_TEMPLATE) {
        if (idx.tag != LV_STR) {
            lv_error(LV_ERR_TYPE, "Template assignment index must be a string",
                     "Use template[\"field\"] = value or template.field = value");
        }
        lv_ptr_store_check(val);
        int i = tpl_find(t.tpl, idx.s->data);
        if (i < 0) {
            lv_errorf(LV_ERR_NAME, "Assign only to declared template fields",
                      "Template field '%s' does not exist", idx.s->data);
        }
        t.tpl->fields[i] = val;
        if (val.tag == LV_BOX) box_promote_owned(val, t.tpl);
        return 1;
    }
    if (t.tag == LV_BLOC) {
        lv_error(LV_ERR_TYPE, "bloc values are immutable", "Build a new bloc, or use a template for data that changes");
    }
    return 0;
}

int lv_data_equal(LV a, LV b) {
    switch (a.tag) {
        case LV_BLOC: return lv_bloc_equal(a, b);
        case LV_BOX: return a.box == b.box;
        case LV_PTR: return a.ptr == b.ptr;
        case LV_TEMPLATE: return a.tpl == b.tpl;
        case LV_DTYPE: return a.dt == b.dt;
        case LV_BLOC_TYPE: return a.bd == b.bd;
        default: return 0;
    }
}

void lv_data_tostring(LvBuf *b, LV v) {
    char tmp[96];
    switch (v.tag) {
        case LV_BLOC:
            lv_buf_add(b, bloc_name(v), (int64_t)strlen(bloc_name(v)));
            lv_buf_add(b, "{", 1);
            for (int i = 0; i < v.bloc->desc->nfields; i++) {
                if (i) lv_buf_add(b, ", ", 2);
                lv_buf_add(b, v.bloc->desc->fields[i], (int64_t)strlen(v.bloc->desc->fields[i]));
                lv_buf_add(b, ": ", 2);
                lv_buf_value(b, v.bloc->vals[i]);
            }
            lv_buf_add(b, "}", 1);
            break;
        case LV_BLOC_TYPE:
            snprintf(tmp, sizeof(tmp), "<bloc %s>", v.bd->name);
            lv_buf_add(b, tmp, (int64_t)strlen(tmp));
            break;
        case LV_DTYPE:
            snprintf(tmp, sizeof(tmp), "<data %s>", v.dt->name);
            lv_buf_add(b, tmp, (int64_t)strlen(tmp));
            break;
        case LV_TEMPLATE:
            lv_buf_add(b, v.tpl->dt->name, (int64_t)strlen(v.tpl->dt->name));
            lv_buf_add(b, "{", 1);
            for (int i = 0; i < v.tpl->n; i++) {
                if (i) lv_buf_add(b, ", ", 2);
                lv_buf_add(b, v.tpl->dt->fields[i], (int64_t)strlen(v.tpl->dt->fields[i]));
                lv_buf_add(b, ": ", 2);
                lv_buf_value(b, v.tpl->fields[i]);
            }
            lv_buf_add(b, "}", 1);
            break;
        case LV_BOX: {
            BoxSlot *s = box_slot(v.box);
            if (!s) {
                snprintf(tmp, sizeof(tmp), luna_data_box_access_ok(v.box) == 5 ? "<box freed>" : "<box invalid>");
            } else {
                snprintf(tmp, sizeof(tmp), "<box len=%zu cap=%zu>", s->len, s->len);
            }
            lv_buf_add(b, tmp, (int64_t)strlen(tmp));
            break;
        }
        case LV_PTR:
            snprintf(tmp, sizeof(tmp), "ptr(0x%llx)", (unsigned long long)v.ptr);
            lv_buf_add(b, tmp, (int64_t)strlen(tmp));
            break;
        default:
            break;
    }
}

const char *lv_data_type_name(LV v) {
    switch (v.tag) {
        case LV_BLOC: return "bloc";
        case LV_BLOC_TYPE: return "bloc_type";
        case LV_DTYPE: return "data_type";
        case LV_TEMPLATE: return "template";
        case LV_BOX: return "box";
        case LV_PTR: return "pointer";
        default: return "unknown";
    }
}

LV_BUILTIN(shape) {
    if (argc != 1) lv_error(LV_ERR_ARGUMENT, "shape() takes exactly 1 argument", "Use shape(value)");
    LV v = argv[0];
    if (v.tag == LV_BLOC) return lv_str(bloc_name(v));
    if (v.tag == LV_BOX) return lv_str("box");
    if (v.tag == LV_TEMPLATE) return lv_str(v.tpl->dt->name);
    if (v.tag == LV_MAP) {
        int found = 0;
        LV tag = lv_map_get(v, lv_str("__tag").s, &found);
        if (found && tag.tag == LV_STR) return tag;
    }
    return lv_null();
}

int lv_data_len(LV v, int64_t *out) {
    if (v.tag == LV_TEMPLATE) {
        *out = v.tpl->n;
        return 1;
    }
    if (v.tag != LV_BOX) return 0;
    box_live(v);
    *out = (int64_t)box_slot(v.box)->len;
    return 1;
}

// ---- unsafe blocks and pointers (port of src/unsafe_runtime.c) -------------

typedef enum { PTR_KIND_ALLOC = 0, PTR_KIND_ADDR = 1 } PointerKind;

typedef struct {
    uintptr_t ptr;
    uintptr_t base;
    size_t cells;
    PointerKind kind;
    int owns_allocation;
    int freed;
    unsigned char *init_flags;
} PointerMeta;

static PointerMeta *ptrs = NULL;
static int ptr_count = 0;
static int ptr_cap = 0;
static int block_depth = 0;
static uintptr_t *defer_ptrs = NULL;
static int defer_count = 0;
static int defer_cap = 0;

int lv_unsafe_depth(void) { return block_depth; }

static void ptrs_reserve(void) {
    if (ptr_count < ptr_cap) return;
    int next = ptr_cap == 0 ? 16 : ptr_cap * 2;
    PointerMeta *grown = realloc(ptrs, sizeof(PointerMeta) * (size_t)next);
    if (!grown) return;
    ptrs = grown;
    ptr_cap = next;
}

static PointerMeta *find_exact(uintptr_t ptr) {
    for (int i = 0; i < ptr_count; i++) {
        if (ptrs[i].ptr == ptr) return &ptrs[i];
    }
    return NULL;
}

static PointerMeta *find_owner(uintptr_t ptr) {
    for (int i = 0; i < ptr_count; i++) {
        PointerMeta *m = &ptrs[i];
        uintptr_t end = m->base + m->cells * sizeof(LV);
        if (ptr >= m->base && ptr < end) return m;
    }
    return NULL;
}

static PointerMeta *resolve_alloc_meta(PointerMeta *m) {
    if (!m || m->kind != PTR_KIND_ALLOC) return m;
    PointerMeta *owner = find_exact(m->base);
    return owner ? owner : m;
}

static void register_alloc_meta(uintptr_t ptr, size_t cells) {
    ptrs_reserve();
    if (ptr_count >= ptr_cap) return;
    PointerMeta *m = &ptrs[ptr_count++];
    m->ptr = ptr;
    m->base = ptr;
    m->cells = cells;
    m->kind = PTR_KIND_ALLOC;
    m->owns_allocation = 1;
    m->freed = 0;
    m->init_flags = calloc(cells, 1);
}

static void register_addr_meta(uintptr_t ptr) {
    if (find_exact(ptr)) return;
    ptrs_reserve();
    if (ptr_count >= ptr_cap) return;
    PointerMeta *m = &ptrs[ptr_count++];
    m->ptr = ptr;
    m->base = ptr;
    m->cells = 1;
    m->kind = PTR_KIND_ADDR;
    m->owns_allocation = 0;
    m->freed = 0;
    m->init_flags = NULL;
}

static void register_alias_meta(uintptr_t ptr, PointerMeta *owner) {
    if (find_exact(ptr)) return;
    ptrs_reserve();
    if (ptr_count >= ptr_cap) return;
    PointerMeta *m = &ptrs[ptr_count++];
    m->ptr = ptr;
    m->base = owner->base;
    m->cells = owner->cells;
    m->kind = owner->kind;
    m->owns_allocation = 0;
    m->freed = owner->freed;
    m->init_flags = owner->init_flags;
}

static const char *unsafe_hint(int code) {
    switch (code) {
        case 1: return "address_of() only works on normal values. If you already have a pointer, use it directly.";
        case 2: return "Pointers must stay inside the current unsafe block. Copy the pointed-to value out instead.";
        case 3: return "That pointer step moved past the allocated buffer. Keep ptr_add() within the allocated slot range.";
        case 4: return "That pointer is no longer valid. Prefer defer(ptr) so cleanup happens once at block exit.";
        case 5: return "address_of() needs a variable name such as address_of(x), not a temporary expression.";
        case 6: return "Nested unsafe blocks are not supported. Merge the inner block into the outer one.";
        case 7: return "Pointers cannot go into lists or maps because those values are GC-managed.";
        case 8: return "alloc() needs a positive whole-number slot count, for example alloc(4).";
        case 9: return "Only compare pointers with <, >, <=, or >= when they come from the same allocation.";
        case 10: return "Only int(ptr) is allowed for pointer casts.";
        case 11: return "Inside unsafe, call builtins only. Move user-defined function calls outside the block.";
        case 12: return "Do not put free(ptr) inside if/while/for/switch branches. Use defer(ptr) or free it unconditionally.";
        case 13: return "Use a pointer returned by alloc(), address_of(), or ptr_add().";
        default: return "Check the unsafe memory rule and the pointer lifetime in this block.";
    }
}

static void report_unsafe(int code) {
    char msg[256];
    luna_mem_error_message(code, msg, sizeof(msg));
    lv_error(LV_ERR_RUNTIME, msg, unsafe_hint(code));
}

static void unsafe_check(int code) {
    if (code != 0) report_unsafe(code);
}

static int is_pointer(LV v) { return v.tag == LV_PTR && find_owner(v.ptr) != NULL; }

static void defer_push(uintptr_t ptr) {
    if (defer_count >= defer_cap) {
        int next = defer_cap == 0 ? 16 : defer_cap * 2;
        uintptr_t *grown = realloc(defer_ptrs, sizeof(uintptr_t) * (size_t)next);
        if (!grown) return;
        defer_ptrs = grown;
        defer_cap = next;
    }
    defer_ptrs[defer_count++] = ptr;
}

static void free_alloc_meta(PointerMeta *m) {
    if (!m || m->kind != PTR_KIND_ALLOC || !m->owns_allocation || m->freed) return;
    lv_gc_root_remove((void *)m->base);
    free((void *)m->base);
    m->freed = 1;
}

static void process_deferred_frees(void) {
    while (defer_count > 0) {
        uintptr_t ptr = defer_ptrs[--defer_count];
        PointerMeta *m = find_exact(ptr);
        if (m && m->kind == PTR_KIND_ALLOC && m->owns_allocation) free_alloc_meta(m);
    }
}

void lv_unsafe_begin(void) {
    unsafe_check(luna_mem_begin());
    block_depth++;
}

void lv_unsafe_end(void) {
    if (block_depth > 0) block_depth--;
    luna_mem_end();
    process_deferred_frees();
}

// R7: a pointer may not be stored in a GC-managed value.
void lv_ptr_store_check(LV v) {
    if (block_depth == 0 || v.tag != LV_PTR || !is_pointer(v)) return;
    unsafe_check(luna_mem_gc_store_ok(v.ptr));
}

// Stores into lists and maps: no raw pointers (R7) and no scoped boxes.
void lv_container_check(LV v) {
    lv_ptr_store_check(v);
    lv_box_escape_check(v, "stored in a list or map");
}

// Assignment to a global: R2 for pointers inside unsafe, and boxes may not
// escape a function through a global.
void lv_global_store_check(LV v, int in_function) {
    if (block_depth > 0 && v.tag == LV_PTR && is_pointer(v)) unsafe_check(luna_mem_escape_ok(v.ptr));
    if (in_function) lv_box_escape_check(v, "assigned to a global from inside a function");
}

// R9: ordered comparison of pointers needs one allocation. op: 0 ==, 1 !=, 2 <, 3 >, 4 <=, 5 >=.
void lv_ptr_compare_check(LV a, LV b, int op) {
    if (!is_pointer(a) || !is_pointer(b)) return;
    unsafe_check(luna_mem_cmp_ok(a.ptr, b.ptr, op));
}

// R10: only int(ptr) may cast a pointer.
void lv_ptr_cast_check(LV v, int target) {
    if (!is_pointer(v)) return;
    unsafe_check(luna_mem_cast_ok(v.ptr, target));
}

// R11: inside unsafe only builtins may be called.
void lv_unsafe_call_check(int is_builtin) {
    unsafe_check(luna_mem_call_ok(is_builtin));
}

int lv_is_pointer(LV v) { return is_pointer(v); }

static void need_unsafe(const char *fn) {
    if (block_depth > 0) return;
    lv_errorf(LV_ERR_RUNTIME, "Wrap pointer operations in unsafe { ... }", "%s() may only be used inside unsafe blocks", fn);
}

LV_BUILTIN(alloc) {
    need_unsafe("alloc");
    if (argc != 1) lv_error(LV_ERR_ARGUMENT, "alloc() expects exactly 1 argument", "Use alloc(slotCount)");
    int is_int = argv[0].tag == LV_INT;
    long cells = is_int ? (long)argv[0].i : 0;
    unsafe_check(luna_mem_alloc_size_ok(cells, is_int));
    LV *mem = calloc((size_t)cells, sizeof(LV));
    if (!mem) lv_error(LV_ERR_RUNTIME, "alloc() failed to reserve memory", "Try a smaller allocation.");
    lv_gc_root_add(mem, mem + cells);
    uintptr_t ptr = (uintptr_t)mem;
    register_alloc_meta(ptr, (size_t)cells);
    luna_mem_track_alloc(ptr, (size_t)cells * sizeof(LV));
    LV v = lv_wrap(LV_PTR);
    v.ptr = ptr;
    return v;
}

LV_BUILTIN(free) {
    if (argc != 1) lv_error(LV_ERR_ARGUMENT, "free() expects exactly 1 argument", "Use free(boxValue) or free(ptr)");
    LV v = argv[0];
    if (v.tag == LV_BOX) {
        BoxSlot *s = box_slot(v.box);
        if (!s) {
            int code = luna_data_box_access_ok(v.box);
            lv_error(LV_ERR_RUNTIME, code == 5 ? "box was already freed (double free)" : "free() expects a live box value",
                     "A box must be freed exactly once");
        }
        if (s->owned_by_template) {
            lv_error(LV_ERR_RUNTIME, "this box belongs to a template and is freed with it", "A box must be freed exactly once");
        }
        box_free_slot(s);
        return lv_null();
    }
    need_unsafe("free");
    if (!is_pointer(v)) {
        lv_error(LV_ERR_TYPE, "free() expects a pointer value",
                 "Pass a pointer returned by alloc(), address_of(), or ptr_add().");
    }
    PointerMeta *m = find_exact(v.ptr);
    if (!m || m->kind != PTR_KIND_ALLOC || !m->owns_allocation) report_unsafe(13);
    unsafe_check(luna_mem_free_ok(v.ptr));
    free_alloc_meta(m);
    return lv_null();
}

LV_BUILTIN(load) {
    need_unsafe("load");
    if (argc != 1) lv_error(LV_ERR_ARGUMENT, "load() expects exactly 1 pointer argument", "Use load(ptr)");
    LV p = argv[0];
    if (!is_pointer(p)) report_unsafe(13);
    PointerMeta *m = find_owner(p.ptr);
    PointerMeta *am = resolve_alloc_meta(m);
    if (m->kind == PTR_KIND_ALLOC) unsafe_check(luna_mem_deref_ok(p.ptr));
    LV *slot = (LV *)p.ptr;
    if (m->kind == PTR_KIND_ALLOC) {
        size_t idx = (p.ptr - am->base) / sizeof(LV);
        return (am->init_flags && am->init_flags[idx]) ? *slot : lv_null();
    }
    return *slot;
}

LV_BUILTIN(deref) { return lb_load(self, argc, argv); }

LV_BUILTIN(store) {
    need_unsafe("store");
    if (argc != 2) lv_error(LV_ERR_ARGUMENT, "store() expects exactly 2 arguments", "Use store(ptr, value)");
    LV p = argv[0];
    if (!is_pointer(p)) report_unsafe(13);
    PointerMeta *m = find_owner(p.ptr);
    PointerMeta *am = resolve_alloc_meta(m);
    if (m->kind == PTR_KIND_ALLOC) unsafe_check(luna_mem_store_ok(p.ptr));
    lv_ptr_store_check(argv[1]);
    LV *slot = (LV *)p.ptr;
    if (m->kind == PTR_KIND_ALLOC) {
        size_t idx = (p.ptr - am->base) / sizeof(LV);
        *slot = argv[1];
        if (am->init_flags) am->init_flags[idx] = 1;
    } else {
        *slot = argv[1];
    }
    return lv_null();
}

LV_BUILTIN(ptr_offset) {
    need_unsafe("ptr_offset");
    if (argc != 2) lv_error(LV_ERR_ARGUMENT, "ptr_offset() expects exactly 2 arguments", "Use ptr_offset(ptr, offset)");
    if (!is_pointer(argv[0]) || argv[1].tag != LV_INT) {
        lv_error(LV_ERR_TYPE, "ptr_add() expects a pointer and an integer offset", "Use ptr_add(buf, 1).");
    }
    PointerMeta *owner = find_owner(argv[0].ptr);
    uintptr_t out = 0;
    size_t byte_offset = (size_t)argv[1].i * sizeof(LV);
    if (owner->kind == PTR_KIND_ALLOC) {
        unsafe_check(luna_mem_ptr_add_ok(argv[0].ptr, byte_offset, &out));
    } else {
        out = argv[0].ptr + byte_offset;
    }
    register_alias_meta(out, owner);
    LV v = lv_wrap(LV_PTR);
    v.ptr = out;
    return v;
}

LV_BUILTIN(ptr_add) { return lb_ptr_offset(self, argc, argv); }

LV_BUILTIN(defer) {
    if (argc != 1) {
        lv_error(LV_ERR_ARGUMENT, "defer() expects exactly 1 argument",
                 "Use defer(close(file)) for general cleanup, or defer(ptr) inside unsafe");
    }
    if (block_depth > 0 && is_pointer(argv[0])) {
        luna_mem_defer(argv[0].ptr);
        defer_push(argv[0].ptr);
        return lv_null();
    }
    lv_error(LV_ERR_ARGUMENT, "defer() expects a call expression",
             "Use defer(close(file)) or another function call to schedule cleanup");
}

// address_of(name): a pointer to the named variable's storage.
LV lv_addr(LV *slot) {
    uintptr_t target = is_pointer(*slot) ? slot->ptr : (uintptr_t)slot;
    unsafe_check(luna_mem_addr_ok(target, 1));
    register_addr_meta((uintptr_t)slot);
    LV v = lv_wrap(LV_PTR);
    v.ptr = (uintptr_t)slot;
    return v;
}

// address_of on anything but a plain name, or with the wrong argument count.
LV lv_addr_invalid(int given) {
    if (block_depth == 0) {
        lv_error(LV_ERR_RUNTIME, "address_of() may only be used inside unsafe blocks", "Wrap pointer operations in unsafe { ... }");
    }
    if (given != 1) lv_error(LV_ERR_ARGUMENT, "address_of() expects exactly 1 argument", "Use address_of(variableName)");
    lv_error(LV_ERR_RUNTIME, "address_of() only works on named values",
             "address_of() needs a variable name such as address_of(x), not a temporary expression.");
}

void luna_data_init(void);

void lv_data_init(void) {
    luna_data_init();
    luna_mem_init();
}
