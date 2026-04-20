// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "value.h"
#include "gc.h"
#include "mystr.h"
#include "env.h"
#include "data_runtime.h"

typedef enum {
    BLOC_FIELD_UNSET = 0,
    BLOC_FIELD_INT,
    BLOC_FIELD_FLOAT,
    BLOC_FIELD_BOOL,
    BLOC_FIELD_CHAR,
    BLOC_FIELD_BLOC,
} BlocFieldKind;

struct BlocTypeDesc {
    const char *name;
    const char **fields;
    unsigned char *kinds;
    unsigned char *offsets;
    BlocTypeDesc **nested_descs;
    int field_count;
    int slot_size;
    int total_size;
    int layout_locked;
    struct BlocTypeDesc *next;
};

static BlocTypeDesc *bloc_registry = NULL;

typedef struct {
    int in_use;
    int ref_count;
    BlocTypeDesc *desc;
    unsigned char bytes[VALUE_BLOC_INLINE_MAX];
} BlocSlot;

#define BLOC_SLOT_MAX 4096
static BlocSlot bloc_slots[BLOC_SLOT_MAX];

typedef struct {
    int in_use;
    int freed;
    int owned_by_template;
    uint64_t scope_id;
    size_t len;
    size_t cap;
    unsigned char *bytes;
} BoxSlot;

struct TemplateObj {
    DataTypeObj *dtype;
    int field_count;
    size_t payload_bytes;
    size_t chunk_bytes;
    Value fields[];
};

#define BOX_SLOT_MAX 4096
#define VALUE_BOX_MAX_BYTES VALUE_BLOC_INLINE_MAX
#define TEMPLATE_MIN_CHUNK_BYTES (16 * 1024)
static BoxSlot box_slots[BOX_SLOT_MAX];

static BlocSlot *bloc_slot_from_handle(uint64_t handle) {
    if (handle == 0 || handle > BLOC_SLOT_MAX) return NULL;
    BlocSlot *slot = &bloc_slots[handle - 1];
    return slot->in_use ? slot : NULL;
}

static uint64_t bloc_alloc_slot(BlocTypeDesc *desc) {
    for (uint64_t i = 0; i < BLOC_SLOT_MAX; i++) {
        if (bloc_slots[i].in_use) continue;
        bloc_slots[i].in_use = 1;
        bloc_slots[i].ref_count = 1;
        bloc_slots[i].desc = desc;
        memset(bloc_slots[i].bytes, 0, sizeof(bloc_slots[i].bytes));
        return i + 1;
    }
    return 0;
}

static BoxSlot *box_slot_from_handle(uint64_t handle) {
    if (handle == 0 || handle > BOX_SLOT_MAX) return NULL;
    BoxSlot *slot = &box_slots[handle - 1];
    return slot->in_use ? slot : NULL;
}

static uint64_t box_alloc_slot(size_t size) {
    for (uint64_t i = 0; i < BOX_SLOT_MAX; i++) {
        if (box_slots[i].in_use) continue;
        unsigned char *bytes = calloc(size, sizeof(unsigned char));
        if (!bytes) return 0;
        box_slots[i].in_use = 1;
        box_slots[i].freed = 0;
        box_slots[i].owned_by_template = 0;
        box_slots[i].scope_id = 0;
        box_slots[i].len = size;
        box_slots[i].cap = size;
        box_slots[i].bytes = bytes;
        return i + 1;
    }
    return 0;
}

static void box_free_slot(BoxSlot *slot) {
    if (!slot || slot->freed) return;
    free(slot->bytes);
    slot->bytes = NULL;
    slot->len = 0;
    slot->cap = 0;
    slot->freed = 1;
    slot->owned_by_template = 0;
    slot->scope_id = 0;
}

static size_t next_pow2_size(size_t n) {
    size_t out = 1;
    while (out < n) out <<= 1;
    return out;
}

static size_t template_chunk_bytes_for_fields(int field_count) {
    size_t payload = sizeof(struct TemplateObj) + sizeof(Value) * (size_t)field_count;
    if (payload < TEMPLATE_MIN_CHUNK_BYTES) return TEMPLATE_MIN_CHUNK_BYTES;
    return next_pow2_size(payload);
}

static int bloc_find_field_index(const BlocTypeDesc *desc, const char *field) {
    if (!desc || !field) return -1;
    for (int i = 0; i < desc->field_count; i++) {
        if (desc->fields[i] == field) return i;
    }
    return -1;
}

static BlocTypeDesc *bloc_find_desc(const char *name) {
    for (BlocTypeDesc *cur = bloc_registry; cur; cur = cur->next) {
        if (cur->name == name) return cur;
    }
    return NULL;
}

static BlocFieldKind bloc_kind_for_value(Value v) {
    switch (v.type) {
        case VAL_INT: return BLOC_FIELD_INT;
        case VAL_FLOAT: return BLOC_FIELD_FLOAT;
        case VAL_BOOL: return BLOC_FIELD_BOOL;
        case VAL_CHAR: return BLOC_FIELD_CHAR;
        case VAL_BLOC: return BLOC_FIELD_BLOC;
        default: return BLOC_FIELD_UNSET;
    }
}

static int bloc_kind_size(BlocFieldKind kind, Value v) {
    switch (kind) {
        case BLOC_FIELD_INT: return (int)sizeof(long long);
        case BLOC_FIELD_FLOAT: return (int)sizeof(double);
        case BLOC_FIELD_BOOL: return (int)sizeof(unsigned char);
        case BLOC_FIELD_CHAR: return (int)sizeof(char);
        case BLOC_FIELD_BLOC: {
            BlocSlot *slot = (v.type == VAL_BLOC) ? bloc_slot_from_handle(v.bloc.handle) : NULL;
            return (slot && slot->desc) ? slot->desc->total_size : 0;
        }
        default: return 0;
    }
}

static int bloc_is_same_schema(const BlocTypeDesc *desc, const char **fields, int field_count) {
    if (!desc || desc->field_count != field_count) return 0;
    for (int i = 0; i < field_count; i++) {
        if (desc->fields[i] != fields[i]) return 0;
    }
    return 1;
}

static int bloc_validate_layout(BlocTypeDesc *desc, int argc, Value *argv, char *msg, size_t msg_len) {
    if (!desc) {
        snprintf(msg, msg_len, "Bloc descriptor is missing");
        return 0;
    }
    if (argc != desc->field_count) {
        snprintf(msg, msg_len, "Bloc '%s' expects %d field value(s), but got %d",
                 desc->name ? desc->name : "<bloc>", desc->field_count, argc);
        return 0;
    }

    int slot_size = 0;
    for (int i = 0; i < argc; i++) {
        BlocFieldKind actual_kind = bloc_kind_for_value(argv[i]);
        if (actual_kind == BLOC_FIELD_UNSET) {
            snprintf(msg, msg_len,
                     "Bloc field '%s' only accepts int, float, bool, char, or another bloc",
                     desc->fields[i]);
            return 0;
        }

        int field_size = bloc_kind_size(actual_kind, argv[i]);
        if (field_size <= 0) {
            snprintf(msg, msg_len, "Bloc field '%s' has an invalid inline size", desc->fields[i]);
            return 0;
        }

        if (desc->layout_locked) {
            if (desc->kinds[i] != (unsigned char)actual_kind) {
                snprintf(msg, msg_len,
                         "Bloc '%s' field '%s' must keep the same primitive layout across constructions",
                         desc->name ? desc->name : "<bloc>", desc->fields[i]);
                return 0;
            }
            BlocSlot *nested_slot = (actual_kind == BLOC_FIELD_BLOC) ? bloc_slot_from_handle(argv[i].bloc.handle) : NULL;
            if (actual_kind == BLOC_FIELD_BLOC && desc->nested_descs[i] != (nested_slot ? nested_slot->desc : NULL)) {
                snprintf(msg, msg_len,
                         "Bloc '%s' field '%s' must use the same nested bloc type",
                         desc->name ? desc->name : "<bloc>", desc->fields[i]);
                return 0;
            }
            continue;
        }

        desc->kinds[i] = (unsigned char)actual_kind;
        if (actual_kind == BLOC_FIELD_BLOC) {
            BlocSlot *nested_slot = bloc_slot_from_handle(argv[i].bloc.handle);
            desc->nested_descs[i] = nested_slot ? nested_slot->desc : NULL;
        } else {
            desc->nested_descs[i] = NULL;
        }
        if (field_size > slot_size) slot_size = field_size;
    }

    if (!desc->layout_locked) {
        if (slot_size <= 0) slot_size = 1;
        int total_size = slot_size * argc;
        if (total_size > VALUE_BLOC_INLINE_MAX) {
            snprintf(msg, msg_len,
                     "Bloc '%s' exceeds the %d-byte cache-line cap",
                     desc->name ? desc->name : "<bloc>", VALUE_BLOC_INLINE_MAX);
            return 0;
        }
        for (int i = 0; i < argc; i++) {
            desc->offsets[i] = (unsigned char)(i * slot_size);
        }
        desc->slot_size = slot_size;
        desc->total_size = total_size;
        desc->layout_locked = 1;
    }

    return 1;
}

static void bloc_store_field(unsigned char *dest, BlocFieldKind kind, Value v, int size) {
    switch (kind) {
        case BLOC_FIELD_INT:
            memcpy(dest, &v.i, sizeof(long long));
            break;
        case BLOC_FIELD_FLOAT:
            memcpy(dest, &v.f, sizeof(double));
            break;
        case BLOC_FIELD_BOOL: {
            unsigned char b = (unsigned char)(v.b ? 1 : 0);
            memcpy(dest, &b, sizeof(unsigned char));
            break;
        }
        case BLOC_FIELD_CHAR:
            memcpy(dest, &v.c, sizeof(char));
            break;
        case BLOC_FIELD_BLOC: {
            BlocSlot *slot = bloc_slot_from_handle(v.bloc.handle);
            if (slot) memcpy(dest, slot->bytes, (size_t)size);
            break;
        }
        default:
            break;
    }
}

static Value bloc_load_field(const BlocTypeDesc *desc, int idx, const unsigned char *src) {
    Value out = value_null();
    if (!desc || idx < 0 || idx >= desc->field_count) return out;

    const unsigned char *field_src = src + desc->offsets[idx];
    switch ((BlocFieldKind)desc->kinds[idx]) {
        case BLOC_FIELD_INT: {
            long long v = 0;
            memcpy(&v, field_src, sizeof(long long));
            out = value_int(v);
            break;
        }
        case BLOC_FIELD_FLOAT: {
            double v = 0.0;
            memcpy(&v, field_src, sizeof(double));
            out = value_float(v);
            break;
        }
        case BLOC_FIELD_BOOL: {
            unsigned char v = 0;
            memcpy(&v, field_src, sizeof(unsigned char));
            out = value_bool(v != 0);
            break;
        }
        case BLOC_FIELD_CHAR: {
            char v = 0;
            memcpy(&v, field_src, sizeof(char));
            out = value_char(v);
            break;
        }
        case BLOC_FIELD_BLOC: {
            out.type = VAL_BLOC;
            out.bloc.handle = bloc_alloc_slot(desc->nested_descs[idx]);
            BlocSlot *slot = bloc_slot_from_handle(out.bloc.handle);
            if (slot && slot->desc) {
                memcpy(slot->bytes, field_src, (size_t)slot->desc->total_size);
            }
            break;
        }
        default:
            break;
    }
    return out;
}

static void string_trace(GCObject *obj, void *ctx) {
    (void)obj;
    (void)ctx;
}

static void string_finalize(GCObject *obj) {
    (void)obj;
}

static void list_items_trace(GCObject *obj, void *ctx) {
    Value *items = (Value *)GC_PAYLOAD(obj);
    size_t count = obj->size / sizeof(Value);
    for (size_t i = 0; i < count; i++) {
        value_gc_mark(&items[i], ctx);
    }
}

static void map_entries_trace(GCObject *obj, void *ctx) {
    MapEntry *entries = (MapEntry *)GC_PAYLOAD(obj);
    size_t count = obj->size / sizeof(MapEntry);
    for (size_t i = 0; i < count; i++) {
        if (!entries[i].occupied) continue;
        value_gc_mark(&entries[i].value, ctx);
    }
}

static void list_trace(GCObject *obj, void *ctx) {
    ListObj *list = (ListObj *)GC_PAYLOAD(obj);
    if (list->items) gc_visit_ref(ctx, (void **)&list->items);
}

static void list_finalize(GCObject *obj) {
    (void)obj;
}

static void dense_list_trace(GCObject *obj, void *ctx) {
    DenseListObj *list = (DenseListObj *)GC_PAYLOAD(obj);
    if (list->data) gc_visit_ref(ctx, (void **)&list->data);
}

static void dense_list_finalize(GCObject *obj) {
    (void)obj;
}

static void map_trace(GCObject *obj, void *ctx) {
    MapObj *map = (MapObj *)GC_PAYLOAD(obj);
    if (map->entries) gc_visit_ref(ctx, (void **)&map->entries);
}

static void map_finalize(GCObject *obj) {
    (void)obj;
}

static void closure_trace(GCObject *obj, void *ctx) {
    ClosureObj *closure = (ClosureObj *)GC_PAYLOAD(obj);
    env_gc_mark_chain(closure->env, ctx);
}

static void closure_finalize(GCObject *obj) {
    ClosureObj *closure = (ClosureObj *)GC_PAYLOAD(obj);
    if (closure->owns_env) env_free_chain(closure->env);
}

static void data_type_trace(GCObject *obj, void *ctx) {
    (void)obj;
    (void)ctx;
}

static void data_type_finalize(GCObject *obj) {
    (void)obj;
}

static void template_trace(GCObject *obj, void *ctx) {
    TemplateObj *templ = (TemplateObj *)GC_PAYLOAD(obj);
    if (templ->dtype) gc_visit_ref(ctx, (void **)&templ->dtype);
    for (int i = 0; i < templ->field_count; i++) {
        value_gc_mark(&templ->fields[i], ctx);
    }
}

static void template_finalize(GCObject *obj) {
    TemplateObj *templ = (TemplateObj *)GC_PAYLOAD(obj);
    for (int i = 0; i < templ->field_count; i++) {
        Value *field = &templ->fields[i];
        if (field->type == VAL_BOX) {
            BoxSlot *slot = box_slot_from_handle(field->box.handle);
            box_free_slot(slot);
        } else {
            value_free(*field);
        }
        field->type = VAL_NULL;
    }
}

static unsigned int map_hash(const char *key) {
    unsigned long long ptr_val = (unsigned long long)key;
    return (unsigned int)((ptr_val >> 4) ^ (ptr_val >> 12));
}

static int next_pow2(int n) {
    int cap = 8;
    while (cap < n) cap <<= 1;
    return cap;
}

static Value *alloc_list_items_buffer(int capacity) {
    size_t bytes = sizeof(Value) * (size_t)capacity;
    if (luna_gc_runtime_enabled()) {
        Value *items = (Value *)luna_gc_alloc(bytes, list_items_trace, NULL);
        memset(items, 0, bytes);
        return items;
    }
    return (Value *)calloc((size_t)capacity, sizeof(Value));
}

static double *alloc_dense_data_buffer(int capacity) {
    size_t bytes = sizeof(double) * (size_t)capacity;
    if (luna_gc_runtime_enabled()) {
        double *data = (double *)luna_gc_alloc(bytes, NULL, NULL);
        memset(data, 0, bytes);
        return data;
    }
    return (double *)malloc(bytes);
}

static MapEntry *alloc_map_entries_buffer(int capacity) {
    size_t bytes = sizeof(MapEntry) * (size_t)capacity;
    if (luna_gc_runtime_enabled()) {
        MapEntry *entries = (MapEntry *)luna_gc_alloc(bytes, map_entries_trace, NULL);
        memset(entries, 0, bytes);
        return entries;
    }
    return (MapEntry *)calloc((size_t)capacity, sizeof(MapEntry));
}

static void gc_note_owner_write(void *payload) {
    if (luna_gc_runtime_enabled() && payload) {
        luna_gc_runtime_remember(payload);
    }
}

static void gc_note_payload_overwrite(void *payload) {
    if (luna_gc_runtime_enabled() && payload) {
        luna_gc_runtime_write_barrier(payload);
    }
}

static void gc_note_owner_write_value(void *payload, const Value *value) {
    if (!luna_gc_runtime_enabled() || !payload || !value || !VALUE_IS_HEAP(*value)) return;

    // Incremental tri-color barrier: if a container links a heap child while marking,
    // shade the child so black->white edges are not lost.
    switch (value->type) {
        case VAL_STRING:
            if (value->string) luna_gc_runtime_write_barrier(value->string);
            break;
        case VAL_LIST:
            if (value->list) luna_gc_runtime_write_barrier(value->list);
            break;
        case VAL_DENSE_LIST:
            if (value->dlist) luna_gc_runtime_write_barrier(value->dlist);
            break;
        case VAL_MAP:
            if (value->map) luna_gc_runtime_write_barrier(value->map);
            break;
        case VAL_CLOSURE:
            if (value->closure) luna_gc_runtime_write_barrier(value->closure);
            break;
        case VAL_DATA_TYPE:
            if (value->dtype) luna_gc_runtime_write_barrier(value->dtype);
            break;
        case VAL_TEMPLATE:
            if (value->template_obj) luna_gc_runtime_write_barrier(value->template_obj);
            break;
        default:
            break;
    }

    // Generational remembered-set hook for old containers pointing to young objects.
    if (!luna_gc_runtime_is_managed_payload(payload)) return;
    luna_gc_runtime_remember(payload);
}

static void gc_note_value_overwrite(const Value *value) {
    if (!luna_gc_runtime_enabled() || !value || !VALUE_IS_HEAP(*value)) return;

    switch (value->type) {
        case VAL_STRING:
            if (value->string) luna_gc_runtime_write_barrier(value->string);
            break;
        case VAL_LIST:
            if (value->list) luna_gc_runtime_write_barrier(value->list);
            break;
        case VAL_DENSE_LIST:
            if (value->dlist) luna_gc_runtime_write_barrier(value->dlist);
            break;
        case VAL_MAP:
            if (value->map) luna_gc_runtime_write_barrier(value->map);
            break;
        case VAL_CLOSURE:
            if (value->closure) luna_gc_runtime_write_barrier(value->closure);
            break;
        case VAL_DATA_TYPE:
            if (value->dtype) luna_gc_runtime_write_barrier(value->dtype);
            break;
        case VAL_TEMPLATE:
            if (value->template_obj) luna_gc_runtime_write_barrier(value->template_obj);
            break;
        default:
            break;
    }
}

static void map_init_storage(MapObj *map, int capacity) {
    map->capacity = next_pow2(capacity);
    map->entries = alloc_map_entries_buffer(map->capacity);
    if (luna_gc_runtime_enabled() && map->entries) {
        luna_gc_runtime_write_barrier(map->entries);
    }
}

static void map_reinsert_entry(MapObj *map, const char *key, Value value) {
    unsigned int idx = map_hash(key) & (map->capacity - 1);
    while (map->entries[idx].occupied) {
        idx = (idx + 1) & (map->capacity - 1);
    }
    map->entries[idx].occupied = 1;
    map->entries[idx].key = key;
    map->entries[idx].value = value;
    map->count++;
}

static void map_grow(MapObj *map, int min_capacity) {
    int old_capacity = map->capacity;
    MapEntry *old_entries = map->entries;
    gc_note_payload_overwrite(old_entries);

    map->entries = NULL;
    map->count = 0;
    map_init_storage(map, min_capacity);
    gc_note_owner_write(map);
    if (luna_gc_runtime_enabled() && map->entries) {
        luna_gc_runtime_write_barrier(map->entries);
    }

    for (int i = 0; i < old_capacity; i++) {
        if (old_entries[i].occupied) {
            map_reinsert_entry(map, old_entries[i].key, old_entries[i].value);
        }
    }
    if (!luna_gc_runtime_enabled()) free(old_entries);
}

static void map_ensure_capacity(MapObj *map) {
    if (!map->entries) {
        map_init_storage(map, 8);
        return;
    }
    if ((map->count + 1) * 10 >= map->capacity * 7) {
        map_grow(map, map->capacity * 2);
    }
}

static MapEntry *map_find_entry(MapObj *map, const char *key) {
    if (!map || !map->entries || map->capacity == 0) return NULL;
    unsigned int idx = map_hash(key) & (map->capacity - 1);
    unsigned int start = idx;
    while (map->entries[idx].occupied) {
        if (map->entries[idx].key == key) return &map->entries[idx];
        idx = (idx + 1) & (map->capacity - 1);
        if (idx == start) break;
    }
    return NULL;
}

static void map_close_delete_gap(MapObj *map, int hole_idx) {
    int idx = (hole_idx + 1) & (map->capacity - 1);
    while (map->entries[idx].occupied) {
        MapEntry entry = map->entries[idx];
        gc_note_value_overwrite(&map->entries[idx].value);
        map->entries[idx].occupied = 0;
        map->count--;
        map_reinsert_entry(map, entry.key, entry.value);
        idx = (idx + 1) & (map->capacity - 1);
    }
}

// Constructor for integer values
Value value_int(long long x) {
    Value v;
    v.type = VAL_INT;
    v.i = x;
    return v;
}

// Constructor for floating point values
Value value_float(double x) {
    Value v;
    v.type = VAL_FLOAT;
    v.f = x;
    return v;
}

Value value_string_len(const char *s, size_t len) {
    Value v;
    v.type = VAL_STRING;
    if (luna_gc_runtime_enabled()) {
        v.string = (StringObj *)luna_gc_alloc(sizeof(StringObj) + len + 1, string_trace, string_finalize);
        v.string->ref_count = 0;
        v.string->chars = (char *)(v.string + 1);
        if (s) {
            memcpy(v.string->chars, s, len + 1);
        } else {
            v.string->chars[0] = '\0';
        }
    } else {
        v.string = malloc(sizeof(StringObj));
        v.string->ref_count = 1;
        if (s) {
            v.string->chars = malloc(len + 1);
            memcpy(v.string->chars, s, len);
            v.string->chars[len] = '\0';
        } else {
            v.string->chars = my_strdup("");
        }
    }
    return v;
}

// Constructor for string values
Value value_string(const char *s) {
    return value_string_len(s, s ? strlen(s) : 0);
}

Value value_string_concat_raw(const char *left, size_t left_len, const char *right, size_t right_len) {
    Value v;
    size_t total_len = left_len + right_len;

    v.type = VAL_STRING;
    if (luna_gc_runtime_enabled()) {
        v.string = (StringObj *)luna_gc_alloc(sizeof(StringObj) + total_len + 1, string_trace, string_finalize);
        v.string->ref_count = 0;
        v.string->chars = (char *)(v.string + 1);
    } else {
        v.string = malloc(sizeof(StringObj));
        v.string->ref_count = 1;
        v.string->chars = malloc(total_len + 1);
    }

    if (left_len) memcpy(v.string->chars, left, left_len);
    if (right_len) memcpy(v.string->chars + left_len, right, right_len);
    v.string->chars[total_len] = '\0';
    return v;
}

Value value_string_repeat_raw(const char *s, size_t len, size_t count) {
    if (!s || count == 0 || len == 0) return value_string("");

    size_t total_len = len * count;
    Value v;
    v.type = VAL_STRING;

    if (luna_gc_runtime_enabled()) {
        v.string = (StringObj *)luna_gc_alloc(sizeof(StringObj) + total_len + 1, string_trace, string_finalize);
        v.string->ref_count = 0;
        v.string->chars = (char *)(v.string + 1);
    } else {
        v.string = malloc(sizeof(StringObj));
        v.string->ref_count = 1;
        v.string->chars = malloc(total_len + 1);
    }

    char *dst = v.string->chars;
    for (size_t i = 0; i < count; i++) {
        memcpy(dst, s, len);
        dst += len;
    }
    *dst = '\0';
    return v;
}

// Constructor for char values
Value value_char(char c) {
    Value v;
    v.type = VAL_CHAR;
    v.c = c;
    return v;
}

// Constructor for boolean values
Value value_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.b = !!b; // Ensure 0 or 1
    return v;
}

Value value_pointer(uintptr_t ptr) {
    Value v;
    v.type = VAL_POINTER;
    v.ptr = ptr;
    return v;
}

Value value_box(size_t size, char *msg, size_t msg_len) {
    if (!data_runtime_check_box_size(size, 0)) {
        if (msg && msg_len > 0) {
            snprintf(msg, msg_len,
                     "box size must be between 1 and %d bytes",
                     VALUE_BOX_MAX_BYTES);
        }
        return value_null();
    }

    uint64_t handle = box_alloc_slot(size);
    if (handle == 0) {
        if (msg && msg_len > 0) {
            snprintf(msg, msg_len, "box allocation failed");
        }
        return value_null();
    }

    data_runtime_track_box(handle);

    Value v;
    v.type = VAL_BOX;
    v.box.handle = handle;
    return v;
}

int value_box_free(Value box, char *msg, size_t msg_len) {
    if (box.type == VAL_BOX && !data_runtime_check_box_free(box.box.handle, 0)) {
        if (msg && msg_len > 0) snprintf(msg, msg_len, "box was already freed (use-after-free)");
        return 0;
    }
    BoxSlot *slot = (box.type == VAL_BOX) ? box_slot_from_handle(box.box.handle) : NULL;
    if (!slot) {
        if (msg && msg_len > 0) snprintf(msg, msg_len, "free() expects a live box value");
        return 0;
    }
    if (slot->freed) {
        if (msg && msg_len > 0) snprintf(msg, msg_len, "box was already freed");
        return 0;
    }

    box_free_slot(slot);
    return 1;
}

size_t value_box_len(Value box) {
    BoxSlot *slot = (box.type == VAL_BOX) ? box_slot_from_handle(box.box.handle) : NULL;
    return slot ? slot->len : 0;
}

size_t value_box_cap(Value box) {
    BoxSlot *slot = (box.type == VAL_BOX) ? box_slot_from_handle(box.box.handle) : NULL;
    return slot ? slot->cap : 0;
}

int value_box_is_live(Value box) {
    BoxSlot *slot = (box.type == VAL_BOX) ? box_slot_from_handle(box.box.handle) : NULL;
    return slot && !slot->freed;
}

void value_box_mark_scope(Value box, uint64_t scope_id) {
    BoxSlot *slot = (box.type == VAL_BOX) ? box_slot_from_handle(box.box.handle) : NULL;
    if (!slot || slot->freed || slot->owned_by_template) return;
    slot->scope_id = scope_id;
}

void value_box_release_scope(uint64_t scope_id) {
    if (!scope_id) return;
    for (int i = 0; i < BOX_SLOT_MAX; i++) {
        BoxSlot *slot = &box_slots[i];
        if (!slot->in_use || slot->freed || slot->owned_by_template) continue;
        if (slot->scope_id == scope_id) box_free_slot(slot);
    }
}

void value_box_promote_to_template(Value box) {
    BoxSlot *slot = (box.type == VAL_BOX) ? box_slot_from_handle(box.box.handle) : NULL;
    if (!slot || slot->freed) return;
    slot->owned_by_template = 1;
    slot->scope_id = 0;
}

Value value_bloc_type(const char *name, const char **fields, int field_count) {
    Value v;
    v.type = VAL_BLOC_TYPE;

    /* Rust-side validation: empty name, duplicate fields, oversize */
    if (!data_runtime_check_bloc(name, fields, field_count, 0)) {
        return value_null();
    }

    BlocTypeDesc *existing = bloc_find_desc(name);
    if (existing) {
        if (!bloc_is_same_schema(existing, fields, field_count)) {
            v.type = VAL_NULL;
            v.i = 0;
            return v;
        }
        v.bloc_type = existing;
        return v;
    }

    BlocTypeDesc *desc = calloc(1, sizeof(BlocTypeDesc));
    if (!desc) return value_null();

    desc->name = name;
    desc->field_count = field_count;
    desc->fields = field_count > 0 ? malloc(sizeof(const char *) * (size_t)field_count) : NULL;
    desc->kinds = field_count > 0 ? calloc((size_t)field_count, sizeof(unsigned char)) : NULL;
    desc->offsets = field_count > 0 ? calloc((size_t)field_count, sizeof(unsigned char)) : NULL;
    desc->nested_descs = field_count > 0 ? calloc((size_t)field_count, sizeof(BlocTypeDesc *)) : NULL;
    for (int i = 0; i < field_count; i++) {
        desc->fields[i] = fields[i];
    }
    desc->next = bloc_registry;
    bloc_registry = desc;

    v.bloc_type = desc;
    return v;
}

int value_bloc_check_construct(Value descriptor, int argc, Value *argv, char *msg, size_t msg_len) {
    if (descriptor.type != VAL_BLOC_TYPE || !descriptor.bloc_type) {
        snprintf(msg, msg_len, "Value is not a bloc constructor");
        return 0;
    }
    return bloc_validate_layout(descriptor.bloc_type, argc, argv, msg, msg_len);
}

Value value_bloc_construct(Value descriptor, int argc, Value *argv) {
    Value out = value_null();
    if (descriptor.type != VAL_BLOC_TYPE || !descriptor.bloc_type) return out;

    out.type = VAL_BLOC;
    out.bloc.handle = bloc_alloc_slot(descriptor.bloc_type);
    BlocSlot *slot = bloc_slot_from_handle(out.bloc.handle);
    if (!slot) return value_null();

    for (int i = 0; i < argc; i++) {
        BlocFieldKind kind = (BlocFieldKind)descriptor.bloc_type->kinds[i];
        int size = bloc_kind_size(kind, argv[i]);
        bloc_store_field(slot->bytes + descriptor.bloc_type->offsets[i], kind, argv[i], size);
    }

    return out;
}

Value value_bloc_get_field(Value bloc, const char *field, int *found) {
    if (found) *found = 0;
    BlocSlot *slot = (bloc.type == VAL_BLOC) ? bloc_slot_from_handle(bloc.bloc.handle) : NULL;
    if (!slot || !slot->desc) return value_null();

    int idx = bloc_find_field_index(slot->desc, field);
    if (idx < 0) return value_null();

    if (found) *found = 1;
    return bloc_load_field(slot->desc, idx, slot->bytes);
}

const char *value_bloc_name(Value v) {
    if (v.type == VAL_BLOC) {
        BlocSlot *slot = bloc_slot_from_handle(v.bloc.handle);
        if (slot && slot->desc) return slot->desc->name;
    }
    if (v.type == VAL_BLOC_TYPE && v.bloc_type) return v.bloc_type->name;
    return NULL;
}

int value_bloc_equal(Value left, Value right) {
    if (left.type != VAL_BLOC || right.type != VAL_BLOC) return 0;
    BlocSlot *left_slot = bloc_slot_from_handle(left.bloc.handle);
    BlocSlot *right_slot = bloc_slot_from_handle(right.bloc.handle);
    if (!left_slot || !right_slot || !left_slot->desc || left_slot->desc != right_slot->desc) return 0;
    return memcmp(left_slot->bytes, right_slot->bytes, (size_t)left_slot->desc->total_size) == 0;
}

// Constructor for empty lists
Value value_list(void) {
    Value v;
    v.type = VAL_LIST;
    v.list = (ListObj *)luna_gc_alloc(sizeof(ListObj), list_trace, list_finalize);
    v.list->ref_count = 0;
    v.list->items = NULL;
    v.list->count = 0;
    v.list->capacity = 0;
    return v;
}

// Constructor for empty dense lists
Value value_dense_list(void) {
    Value v;
    v.type = VAL_DENSE_LIST;
    v.dlist = (DenseListObj *)luna_gc_alloc(sizeof(DenseListObj), dense_list_trace, dense_list_finalize);
    v.dlist->ref_count = 0;
    v.dlist->data = NULL;
    v.dlist->count = 0;
    v.dlist->capacity = 0;
    return v;
}

Value value_map(void) {
    Value v;
    v.type = VAL_MAP;
    v.map = (MapObj *)luna_gc_alloc(sizeof(MapObj), map_trace, map_finalize);
    v.map->ref_count = 0;
    v.map->entries = NULL;
    v.map->count = 0;
    v.map->capacity = 0;
    map_init_storage(v.map, 8);
    return v;
}

Value value_closure(struct AstNode *funcdef, struct Env *env, int owns_env) {
    Value v;
    v.type = VAL_CLOSURE;
    v.closure = (ClosureObj *)luna_gc_alloc(sizeof(ClosureObj), closure_trace, closure_finalize);
    v.closure->ref_count = 0;
    v.closure->funcdef = funcdef;
    v.closure->env = env;
    v.closure->owns_env = owns_env;
    return v;
}

Value value_data_type(const char *name, const char **fields, int field_count, int is_template) {
    /* Rust-side template schema registration */
    if (is_template) {
        if (!data_runtime_check_template_register(name, fields, field_count, 0)) {
            return value_null();
        }
    }

    Value v;
    v.type = VAL_DATA_TYPE;
    if (luna_gc_runtime_enabled()) {
        v.dtype = (DataTypeObj *)luna_gc_alloc(sizeof(DataTypeObj) + sizeof(const char *) * (size_t)field_count,
                                               data_type_trace, data_type_finalize);
        v.dtype->ref_count = 0;
        v.dtype->fields = (const char **)(v.dtype + 1);
    } else {
        v.dtype = malloc(sizeof(DataTypeObj));
        v.dtype->ref_count = 1;
        v.dtype->fields = field_count > 0 ? malloc(sizeof(const char *) * (size_t)field_count) : NULL;
    }
    v.dtype->name = name;
    v.dtype->field_count = field_count;
    v.dtype->is_template = is_template;
    for (int i = 0; i < field_count; i++) {
        v.dtype->fields[i] = fields[i];
    }
    return v;
}

Value value_template_from_dtype(Value dtype, int argc, Value *argv, char *msg, size_t msg_len) {
    Value out = value_null();
    if (dtype.type != VAL_DATA_TYPE || !dtype.dtype || !dtype.dtype->is_template) {
        if (msg && msg_len > 0) snprintf(msg, msg_len, "Value is not a template constructor");
        return out;
    }
    /* Rust-side arity check */
    if (!data_runtime_check_template_arity(dtype.dtype->name, argc, 0)) {
        if (msg && msg_len > 0) {
            snprintf(msg, msg_len, "Template '%s' expects %d field value(s), but got %d",
                     dtype.dtype->name ? dtype.dtype->name : "<template>", dtype.dtype->field_count, argc);
        }
        return out;
    }
    if (argc != dtype.dtype->field_count) {
        if (msg && msg_len > 0) {
            snprintf(msg, msg_len, "Template '%s' expects %d field value(s), but got %d",
                     dtype.dtype->name ? dtype.dtype->name : "<template>", dtype.dtype->field_count, argc);
        }
        return out;
    }

    size_t chunk_bytes = template_chunk_bytes_for_fields(argc);
    TemplateObj *templ = (TemplateObj *)luna_gc_alloc(chunk_bytes, template_trace, template_finalize);
    if (!templ) {
        if (msg && msg_len > 0) snprintf(msg, msg_len, "template allocation failed");
        return out;
    }

    templ->dtype = dtype.dtype;
    templ->field_count = argc;
    templ->payload_bytes = sizeof(struct TemplateObj) + sizeof(Value) * (size_t)argc;
    templ->chunk_bytes = chunk_bytes;
    for (int i = 0; i < argc; i++) {
        templ->fields[i] = value_copy(argv[i]);
        if (templ->fields[i].type == VAL_BOX) {
            data_runtime_check_containment(DATA_KIND_TEMPLATE, DATA_KIND_BOX, 0);
            value_box_promote_to_template(templ->fields[i]);
        }
    }

    out.type = VAL_TEMPLATE;
    out.template_obj = templ;
    return out;
}

static int template_find_field_index(const TemplateObj *templ, const char *field) {
    if (!templ || !templ->dtype || !field) return -1;
    for (int i = 0; i < templ->dtype->field_count; i++) {
        if (templ->dtype->fields[i] == field) return i;
    }
    return -1;
}

Value value_template_get_field(Value template_value, const char *field, int *found) {
    if (found) *found = 0;
    if (template_value.type != VAL_TEMPLATE || !template_value.template_obj) return value_null();
    int idx = template_find_field_index(template_value.template_obj, field);
    if (idx < 0) return value_null();
    if (found) *found = 1;
    return value_copy(template_value.template_obj->fields[idx]);
}

Value *value_template_field_slot(Value *template_value, const char *field) {
    if (!template_value || template_value->type != VAL_TEMPLATE || !template_value->template_obj) return NULL;
    int idx = template_find_field_index(template_value->template_obj, field);
    if (idx < 0) return NULL;
    return &template_value->template_obj->fields[idx];
}

int value_template_set_field(Value *template_value, const char *field, Value *rhs, char *msg, size_t msg_len) {
    if (!template_value || template_value->type != VAL_TEMPLATE || !template_value->template_obj) {
        if (msg && msg_len > 0) snprintf(msg, msg_len, "Target is not a template value");
        return 0;
    }
    int idx = template_find_field_index(template_value->template_obj, field);
    if (idx < 0) {
        if (msg && msg_len > 0) snprintf(msg, msg_len, "Template field '%s' does not exist", field ? field : "");
        return 0;
    }
    Value *slot = &template_value->template_obj->fields[idx];
    value_free(*slot);
    *slot = value_move(rhs);
    if (slot->type == VAL_BOX) {
        data_runtime_check_containment(DATA_KIND_TEMPLATE, DATA_KIND_BOX, 0);
        value_box_promote_to_template(*slot);
    }
    return 1;
}

const char *value_template_name(Value v) {
    if (v.type != VAL_TEMPLATE || !v.template_obj || !v.template_obj->dtype) return NULL;
    return v.template_obj->dtype->name;
}

int value_template_len(Value v) {
    if (v.type != VAL_TEMPLATE || !v.template_obj) return 0;
    return v.template_obj->field_count;
}

// Constructor for native functions
Value value_native(NativeFunc fn) {
    Value v;
    v.type = VAL_NATIVE;
    v.native = (void *)fn;
    return v;
}

// Constructor for file handles
Value value_file(FILE *f) {
    Value v;
    v.type = VAL_FILE;
    v.file = f;
    return v;
}

// Constructor for null/void values
Value value_null(void) {
    Value v;
    v.type = VAL_NULL;
    v.i = 0;
    return v;
}

// Frees memory associated with a heap-allocated Value (slow path)
// Only called for STRING, LIST, DENSE_LIST via the inline wrapper in value.h
void _value_free_refcount(Value v) {
    if (luna_gc_runtime_enabled()) {
        (void)v;
        return;
    }

    struct timespec start_ts;
    struct timespec end_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    if (v.type == VAL_STRING && v.string) {
        v.string->ref_count--;
        if (v.string->ref_count == 0) {
            free(v.string->chars);
            free(v.string);
        }
    } else if (v.type == VAL_LIST && v.list) {
        v.list->ref_count--;
        if (v.list->ref_count == 0) {
            for (int i = 0; i < v.list->count; i++) {
                value_free(v.list->items[i]);
            }
            free(v.list->items);
            free(v.list);
        }
    } else if (v.type == VAL_DENSE_LIST && v.dlist) {
        v.dlist->ref_count--;
        if (v.dlist->ref_count == 0) {
            free(v.dlist->data);
            free(v.dlist);
        }
    } else if (v.type == VAL_MAP && v.map) {
        v.map->ref_count--;
        if (v.map->ref_count == 0) {
            for (int i = 0; i < v.map->capacity; i++) {
                if (v.map->entries[i].occupied) {
                    value_free(v.map->entries[i].value);
                }
            }
            free(v.map->entries);
            free(v.map);
        }
    } else if (v.type == VAL_CLOSURE && v.closure) {
        v.closure->ref_count--;
        if (v.closure->ref_count == 0) {
            if (v.closure->owns_env) env_free_chain(v.closure->env);
            free(v.closure);
        }
    } else if (v.type == VAL_DATA_TYPE && v.dtype) {
        v.dtype->ref_count--;
        if (v.dtype->ref_count == 0) {
            free((void *)v.dtype->fields);
            free(v.dtype);
        }
    } else if (v.type == VAL_TEMPLATE) {
        /* GC-managed; nothing to free in the refcount fallback path. */
    } else if (v.type == VAL_BLOC) {
        BlocSlot *slot = bloc_slot_from_handle(v.bloc.handle);
        if (slot) {
            slot->ref_count--;
            if (slot->ref_count <= 0) {
                slot->in_use = 0;
                slot->ref_count = 0;
                slot->desc = NULL;
                memset(slot->bytes, 0, sizeof(slot->bytes));
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    long sec = end_ts.tv_sec - start_ts.tv_sec;
    long nsec = end_ts.tv_nsec - start_ts.tv_nsec;
    if (nsec < 0) {
        sec--;
        nsec += 1000000000L;
    }
    unsigned long long elapsed_ns =
        (unsigned long long)sec * 1000000000ULL +
        (unsigned long long)nsec;
    gc_stats_record_pause_ns(elapsed_ns);
}

// Creates a deep copy of a Value (ref count bump for heap types)
Value _value_copy_refcount(Value v) {
    if (luna_gc_runtime_enabled()) {
        return v;
    }

    switch (v.type) {
        case VAL_STRING:
            if (v.string) v.string->ref_count++;
            break;
        case VAL_LIST:
            if (v.list) v.list->ref_count++;
            break;
        case VAL_DENSE_LIST:
            if (v.dlist) v.dlist->ref_count++;
            break;
        case VAL_MAP:
            if (v.map) v.map->ref_count++;
            break;
        case VAL_CLOSURE:
            if (v.closure) v.closure->ref_count++;
            break;
        case VAL_DATA_TYPE:
            if (v.dtype) v.dtype->ref_count++;
            break;
        case VAL_TEMPLATE:
            break;
        case VAL_BLOC: {
            BlocSlot *slot = bloc_slot_from_handle(v.bloc.handle);
            if (slot) slot->ref_count++;
            break;
        }
        default:
            break;
    }
    return v;
}

void value_gc_mark(Value *value, void *ctx) {
    if (!value || !VALUE_IS_HEAP(*value)) return;

    switch (value->type) {
        case VAL_STRING:
            if (value->string) gc_visit_ref(ctx, (void **)&value->string);
            break;
        case VAL_LIST:
            if (value->list) gc_visit_ref(ctx, (void **)&value->list);
            break;
        case VAL_DENSE_LIST:
            if (value->dlist) gc_visit_ref(ctx, (void **)&value->dlist);
            break;
        case VAL_MAP:
            if (value->map) gc_visit_ref(ctx, (void **)&value->map);
            break;
        case VAL_CLOSURE:
            if (value->closure) gc_visit_ref(ctx, (void **)&value->closure);
            break;
        case VAL_DATA_TYPE:
            if (value->dtype) gc_visit_ref(ctx, (void **)&value->dtype);
            break;
        case VAL_TEMPLATE:
            if (value->template_obj) gc_visit_ref(ctx, (void **)&value->template_obj);
            break;
        default:
            break;
    }
}

// Converts a Value to a string representation (for printing)
char *value_to_string(Value v) {
    char buf[128];
    switch (v.type) {
        case VAL_INT:
            snprintf(buf, 128, "%lld", v.i); // Use lld for long long
            return my_strdup(buf);
        case VAL_FLOAT:
            snprintf(buf, 128, "%.6g", v.f);
            return my_strdup(buf);
        case VAL_BOOL:
            return my_strdup(v.b ? "true" : "false");
        case VAL_POINTER:
            snprintf(buf, sizeof(buf), "ptr(0x%llx)", (unsigned long long)v.ptr);
            return my_strdup(buf);
        case VAL_BLOC_TYPE:
            if (v.bloc_type && v.bloc_type->name) {
                size_t len = strlen(v.bloc_type->name) + 8;
                char *res = malloc(len);
                snprintf(res, len, "<bloc %s>", v.bloc_type->name);
                return res;
            }
            return my_strdup("<bloc>");
        case VAL_BLOC: {
            size_t cap = 64;
            size_t pos = 0;
            char *res = malloc(cap);
            BlocSlot *slot = bloc_slot_from_handle(v.bloc.handle);
            BlocTypeDesc *desc = slot ? slot->desc : NULL;
            const char *name = (desc && desc->name) ? desc->name : "bloc";
            size_t name_len = strlen(name);
            while (pos + name_len + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
            memcpy(res + pos, name, name_len);
            pos += name_len;
            res[pos++] = '{';
            if (desc && slot) {
                for (int i = 0; i < desc->field_count; i++) {
                    if (i > 0) {
                        while (pos + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                        res[pos++] = ',';
                        res[pos++] = ' ';
                    }
                    size_t field_len = strlen(desc->fields[i]);
                    while (pos + field_len + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                    memcpy(res + pos, desc->fields[i], field_len);
                    pos += field_len;
                    res[pos++] = ':';
                    res[pos++] = ' ';
                    Value field = bloc_load_field(desc, i, slot->bytes);
                    char *field_str = value_to_string(field);
                    size_t field_str_len = strlen(field_str);
                    while (pos + field_str_len + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                    memcpy(res + pos, field_str, field_str_len);
                    pos += field_str_len;
                    free(field_str);
                }
            }
            while (pos + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
            res[pos++] = '}';
            res[pos] = '\0';
            return res;
        }
        case VAL_BOX: {
            size_t len = 48;
            char *res = malloc(len);
            BoxSlot *slot = box_slot_from_handle(v.box.handle);
            if (!slot) {
                snprintf(res, len, "<box invalid>");
            } else if (slot->freed) {
                snprintf(res, len, "<box freed>");
            } else {
                snprintf(res, len, "<box len=%zu cap=%zu>", slot->len, slot->cap);
            }
            return res;
        }
        case VAL_CHAR:
            snprintf(buf, 128, "%c", v.c);
            return my_strdup(buf);
        case VAL_NATIVE:
            return my_strdup("<native function>");
        case VAL_FUNCTION:
            return my_strdup("<function>");
        case VAL_CLOSURE:
            return my_strdup("<closure>");
        case VAL_DATA_TYPE:
            if (v.dtype && v.dtype->name) {
                size_t len = strlen(v.dtype->name) + 8;
                char *res = malloc(len);
                snprintf(res, len, "<data %s>", v.dtype->name);
                return res;
            }
            return my_strdup("<data>");
        case VAL_TEMPLATE: {
            TemplateObj *templ = v.template_obj;
            const char *name = (templ && templ->dtype && templ->dtype->name) ? templ->dtype->name : "template";
            size_t cap = 96, pos = 0;
            char *res = malloc(cap);
            size_t name_len = strlen(name);
            while (pos + name_len + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
            memcpy(res + pos, name, name_len);
            pos += name_len;
            res[pos++] = '{';
            if (templ && templ->dtype) {
                for (int i = 0; i < templ->field_count; i++) {
                    if (i > 0) { while (pos + 2 >= cap) { cap *= 2; res = realloc(res, cap); } res[pos++] = ','; res[pos++] = ' '; }
                    const char *field_name = templ->dtype->fields[i] ? templ->dtype->fields[i] : "";
                    size_t field_len = strlen(field_name);
                    while (pos + field_len + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                    memcpy(res + pos, field_name, field_len);
                    pos += field_len;
                    res[pos++] = ':';
                    res[pos++] = ' ';
                    char *field_str = value_to_string(templ->fields[i]);
                    size_t field_str_len = strlen(field_str);
                    while (pos + field_str_len + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                    memcpy(res + pos, field_str, field_str_len);
                    pos += field_str_len;
                    free(field_str);
                }
            }
            while (pos + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
            res[pos++] = '}';
            res[pos] = '\0';
            return res;
        }
        case VAL_FILE:
            if (v.file) return my_strdup("<file handle>");
            else return my_strdup("<closed file>");
        case VAL_STRING:
            if (v.string && v.string->chars) {
                return my_strdup(v.string->chars);
            } else {
                return my_strdup("");
            }
        case VAL_LIST: {
            // O(n) list-to-string: track write position to avoid O(n²) strcat
            size_t cap = 64, pos = 0;
            char *res = malloc(cap);
            res[pos++] = '[';
            if (v.list) {
                for (int i = 0; i < v.list->count; i++) {
                    if (i > 0) { 
                        if (pos + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                        res[pos++] = ','; res[pos++] = ' '; 
                    }
                    char *vs = value_to_string(v.list->items[i]);
                    size_t vl = strlen(vs);
                    while (pos + vl + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                    memcpy(res + pos, vs, vl);
                    pos += vl;
                    free(vs);
                }
            }
            res[pos++] = ']';
            res[pos] = '\0';
            return res;
        }
        case VAL_DENSE_LIST: {
            // O(n) dense-list-to-string: track write position
            size_t cap = 64, pos = 0;
            char *res = malloc(cap);
            res[pos++] = 'd'; res[pos++] = '[';
            if (v.dlist) {
                for (int i = 0; i < v.dlist->count; i++) {
                    if (i > 0) {
                        if (pos + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                        res[pos++] = ','; res[pos++] = ' ';
                    }
                    char tbuf[64];
                    int tl = snprintf(tbuf, 64, "%.6g", v.dlist->data[i]);
                    while (pos + tl + 2 >= cap) { cap *= 2; res = realloc(res, cap); }
                    memcpy(res + pos, tbuf, tl);
                    pos += tl;
                }
            }
            res[pos++] = ']';
            res[pos] = '\0';
            return res;
        }
        case VAL_MAP: {
            size_t cap = 64, pos = 0;
            char *res = malloc(cap);
            res[pos++] = '{';
            int first = 1;
            if (v.map) {
                for (int i = 0; i < v.map->capacity; i++) {
                    if (!v.map->entries[i].occupied) continue;
                    const char *key = v.map->entries[i].key ? v.map->entries[i].key : "";
                    char *vs = value_to_string(v.map->entries[i].value);
                    size_t kl = strlen(key), vl = strlen(vs);
                    size_t needed = pos + (first ? 0 : 2) + kl + vl + 6;
                    while (needed >= cap) { cap *= 2; res = realloc(res, cap); }
                    if (!first) { res[pos++] = ','; res[pos++] = ' '; }
                    res[pos++] = '"';
                    memcpy(res + pos, key, kl);
                    pos += kl;
                    res[pos++] = '"';
                    res[pos++] = ':';
                    res[pos++] = ' ';
                    memcpy(res + pos, vs, vl);
                    pos += vl;
                    free(vs);
                    first = 0;
                }
            }
            res[pos++] = '}';
            res[pos] = '\0';
            return res;
        }
        default:
            return my_strdup("null");
    }
}

// Prints a Value directly to a file stream without allocating memory.
// For primitives this is zero-alloc. Lists still recurse but avoid the O(n²) strcat.
void value_fprint(FILE *f, Value v) {
    switch (v.type) {
        case VAL_INT:    fprintf(f, "%lld", v.i); break;
        case VAL_FLOAT:  fprintf(f, "%.6g", v.f); break;
        case VAL_BOOL:   fputs(v.b ? "true" : "false", f); break;
        case VAL_POINTER: fprintf(f, "ptr(0x%llx)", (unsigned long long)v.ptr); break;
        case VAL_BLOC_TYPE:
            if (v.bloc_type && v.bloc_type->name) fprintf(f, "<bloc %s>", v.bloc_type->name);
            else fputs("<bloc>", f);
            break;
        case VAL_BLOC:
        {
            BlocSlot *slot = bloc_slot_from_handle(v.bloc.handle);
            BlocTypeDesc *desc = slot ? slot->desc : NULL;
            if (desc && desc->name) fputs(desc->name, f);
            else fputs("bloc", f);
            fputc('{', f);
            if (desc && slot) {
                for (int i = 0; i < desc->field_count; i++) {
                    if (i > 0) fputs(", ", f);
                    fputs(desc->fields[i], f);
                    fputs(": ", f);
                    Value field = bloc_load_field(desc, i, slot->bytes);
                    value_fprint(f, field);
                }
            }
            fputc('}', f);
            break;
        }
        case VAL_BOX: {
            BoxSlot *slot = box_slot_from_handle(v.box.handle);
            if (!slot) fputs("<box invalid>", f);
            else if (slot->freed) fputs("<box freed>", f);
            else fprintf(f, "<box len=%zu cap=%zu>", slot->len, slot->cap);
            break;
        }
        case VAL_CHAR:   fputc(v.c, f); break;
        case VAL_NATIVE: fputs("<native function>", f); break;
        case VAL_FILE:   fputs(v.file ? "<file handle>" : "<closed file>", f); break;
        case VAL_NULL:   fputs("null", f); break;
        case VAL_FUNCTION: fputs("<function>", f); break;
        case VAL_CLOSURE: fputs("<closure>", f); break;
        case VAL_DATA_TYPE:
            if (v.dtype && v.dtype->name) fprintf(f, "<data %s>", v.dtype->name);
            else fputs("<data>", f);
            break;
        case VAL_TEMPLATE: {
            TemplateObj *templ = v.template_obj;
            const char *name = (templ && templ->dtype && templ->dtype->name) ? templ->dtype->name : "template";
            fputs(name, f);
            fputc('{', f);
            if (templ && templ->dtype) {
                for (int i = 0; i < templ->field_count; i++) {
                    if (i > 0) fputs(", ", f);
                    fputs(templ->dtype->fields[i], f);
                    fputs(": ", f);
                    value_fprint(f, templ->fields[i]);
                }
            }
            fputc('}', f);
            break;
        }
        case VAL_STRING:
            if (v.string && v.string->chars) fputs(v.string->chars, f);
            break;
        case VAL_LIST:
            fputc('[', f);
            if (v.list) {
                for (int i = 0; i < v.list->count; i++) {
                    if (i > 0) fputs(", ", f);
                    value_fprint(f, v.list->items[i]);
                }
            }
            fputc(']', f);
            break;
        case VAL_DENSE_LIST:
            fputs("d[", f);
            if (v.dlist) {
                for (int i = 0; i < v.dlist->count; i++) {
                    if (i > 0) fputs(", ", f);
                    fprintf(f, "%.6g", v.dlist->data[i]);
                }
            }
            fputc(']', f);
            break;
        case VAL_MAP: {
            fputc('{', f);
            int first = 1;
            if (v.map) {
                for (int i = 0; i < v.map->capacity; i++) {
                    if (!v.map->entries[i].occupied) continue;
                    if (!first) fputs(", ", f);
                    fprintf(f, "\"%s\": ", v.map->entries[i].key ? v.map->entries[i].key : "");
                    value_fprint(f, v.map->entries[i].value);
                    first = 0;
                }
            }
            fputc('}', f);
            break;
        }
    }
}

// Appends a value to a list, resizing capacity if needed
void value_list_append(Value *list, Value v) {
    if (list->type != VAL_LIST || !list->list) {
        return;
    }
    if (list->list->count >= list->list->capacity) {
        int n = list->list->capacity == 0 ? 4 : list->list->capacity * 2;
        if (luna_gc_runtime_enabled()) {
            Value *old_items = list->list->items;
            Value *grown = alloc_list_items_buffer(n);
            if (old_items) {
                memcpy(grown, old_items, sizeof(Value) * (size_t)list->list->count);
            }
            gc_note_payload_overwrite(old_items);
            list->list->items = grown;
            gc_note_owner_write(list->list);
            if (grown) {
                luna_gc_runtime_write_barrier(grown);
            }
        } else {
            list->list->items = realloc(list->list->items, sizeof(Value) * (size_t)n);
        }
        list->list->capacity = n;
    }
    gc_note_owner_write_value(list->list->items, &v);
    list->list->items[list->list->count++] = value_copy(v);
}

// Appends a value to a list with move semantics (takes ownership, no copy)
void value_list_append_move(Value *list, Value *v) {
    if (list->type != VAL_LIST || !list->list) {
        return;
    }
    if (list->list->count >= list->list->capacity) {
        int n = list->list->capacity == 0 ? 4 : list->list->capacity * 2;
        if (luna_gc_runtime_enabled()) {
            Value *old_items = list->list->items;
            Value *grown = alloc_list_items_buffer(n);
            if (old_items) {
                memcpy(grown, old_items, sizeof(Value) * (size_t)list->list->count);
            }
            gc_note_payload_overwrite(old_items);
            list->list->items = grown;
            gc_note_owner_write(list->list);
            if (grown) {
                luna_gc_runtime_write_barrier(grown);
            }
        } else {
            list->list->items = realloc(list->list->items, sizeof(Value) * (size_t)n);
        }
        list->list->capacity = n;
    }
    gc_note_owner_write_value(list->list->items, v);
    list->list->items[list->list->count++] = *v;
    v->type = VAL_NULL;
}

// Appends a double directly to a dense list
void value_dlist_append(Value *list, double v) {
    if (list->type != VAL_DENSE_LIST || !list->dlist) {
        return;
    }
    if (list->dlist->count >= list->dlist->capacity) {
        int n = list->dlist->capacity == 0 ? 4 : list->dlist->capacity * 2;
        if (luna_gc_runtime_enabled()) {
            double *old_data = list->dlist->data;
            double *grown = alloc_dense_data_buffer(n);
            if (old_data) {
                memcpy(grown, old_data, sizeof(double) * (size_t)list->dlist->count);
            }
            gc_note_payload_overwrite(old_data);
            list->dlist->data = grown;
            gc_note_owner_write(list->dlist);
        } else {
            list->dlist->data = realloc(list->dlist->data, sizeof(double) * (size_t)n);
        }
        list->dlist->capacity = n;
    }
    list->dlist->data[list->dlist->count++] = v;
}

void value_map_set(Value *map, const char *key, Value v) {
    if (!map || map->type != VAL_MAP || !map->map || !key) return;
    map_ensure_capacity(map->map);
    MapEntry *entry = map_find_entry(map->map, key);
    if (entry) {
        gc_note_owner_write_value(map->map->entries, &v);
        gc_note_value_overwrite(&entry->value);
        value_free(entry->value);
        entry->value = value_copy(v);
        return;
    }

    unsigned int idx = map_hash(key) & (map->map->capacity - 1);
    while (map->map->entries[idx].occupied) {
        idx = (idx + 1) & (map->map->capacity - 1);
    }
    gc_note_owner_write_value(map->map->entries, &v);
    map->map->entries[idx].occupied = 1;
    map->map->entries[idx].key = key;
    map->map->entries[idx].value = value_copy(v);
    map->map->count++;
}

void value_map_set_move(Value *map, const char *key, Value *v) {
    if (!map || map->type != VAL_MAP || !map->map || !key || !v) return;
    map_ensure_capacity(map->map);
    MapEntry *entry = map_find_entry(map->map, key);
    if (entry) {
        gc_note_owner_write_value(map->map->entries, v);
        gc_note_value_overwrite(&entry->value);
        value_free(entry->value);
        entry->value = *v;
        v->type = VAL_NULL;
        return;
    }

    unsigned int idx = map_hash(key) & (map->map->capacity - 1);
    while (map->map->entries[idx].occupied) {
        idx = (idx + 1) & (map->map->capacity - 1);
    }
    gc_note_owner_write_value(map->map->entries, v);
    map->map->entries[idx].occupied = 1;
    map->map->entries[idx].key = key;
    map->map->entries[idx].value = *v;
    map->map->count++;
    v->type = VAL_NULL;
}

Value *value_map_get(Value *map, const char *key) {
    MapEntry *entry = (!map || map->type != VAL_MAP) ? NULL : map_find_entry(map->map, key);
    return entry ? &entry->value : NULL;
}

int value_map_has(Value *map, const char *key) {
    return value_map_get(map, key) != NULL;
}

int value_map_delete(Value *map, const char *key) {
    if (!map || map->type != VAL_MAP || !map->map || !key) return 0;
    MapEntry *entry = map_find_entry(map->map, key);
    if (!entry) return 0;
    int idx = (int)(entry - map->map->entries);
    gc_note_owner_write(map->map->entries);
    gc_note_value_overwrite(&entry->value);
    value_free(entry->value);
    entry->occupied = 0;
    entry->key = NULL;
    map->map->count--;
    map_close_delete_gap(map->map, idx);
    return 1;
}

Value value_map_keys(Value map) {
    Value keys = value_list();
    if (map.type != VAL_MAP || !map.map) return keys;
    for (int i = 0; i < map.map->capacity; i++) {
        if (!map.map->entries[i].occupied) continue;
        value_list_append(&keys, value_string(map.map->entries[i].key));
    }
    return keys;
}

Value value_map_values(Value map) {
    Value out = value_list();
    if (map.type != VAL_MAP || !map.map || !map.map->entries) return out;
    for (int i = 0; i < map.map->capacity; i++) {
        if (!map.map->entries[i].occupied) continue;
        value_list_append(&out, map.map->entries[i].value);
    }
    return out;
}

Value value_map_items(Value map) {
    Value out = value_list();
    if (map.type != VAL_MAP || !map.map || !map.map->entries) return out;
    for (int i = 0; i < map.map->capacity; i++) {
        if (!map.map->entries[i].occupied) continue;
        Value pair = value_list();
        value_list_append(&pair, value_string(map.map->entries[i].key));
        value_list_append(&pair, map.map->entries[i].value);
        value_list_append_move(&out, &pair);
    }
    return out;
}