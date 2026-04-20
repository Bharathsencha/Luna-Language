// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef VALUE_H
#define VALUE_H
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Value Value; // Forward decl
typedef struct BlocTypeDesc BlocTypeDesc;
typedef struct TemplateObj TemplateObj;

// Forward declaration for Environment and AstNode to avoid circular dependencies
struct Env;
struct AstNode;

typedef enum {
    // Non-heap types (plain struct copy, no ref counting)
    VAL_INT,
    VAL_FLOAT,   
    VAL_CHAR,   
    VAL_BOOL,
    VAL_POINTER,
    VAL_BLOC_TYPE,
    VAL_BLOC,
    VAL_BOX,
    VAL_NATIVE, 
    VAL_FILE,   // File Handle Type
    VAL_NULL,
    VAL_FUNCTION, 
    // Heap types (ref counted)
    VAL_STRING,
    VAL_LIST,
    VAL_DENSE_LIST, // Added for high-performance SIMD/Matrix math
    VAL_MAP,
    VAL_CLOSURE,
    VAL_DATA_TYPE,
    VAL_TEMPLATE,
} ValueType;

#define VALUE_IS_HEAP(v) \
    ((v).type == VAL_STRING || (v).type == VAL_LIST || (v).type == VAL_DENSE_LIST || \
     (v).type == VAL_MAP || (v).type == VAL_CLOSURE || (v).type == VAL_DATA_TYPE || \
     (v).type == VAL_BLOC || (v).type == VAL_TEMPLATE)

typedef struct {
    int ref_count;
    char *chars;
} StringObj;

typedef struct {
    int ref_count;
    struct Value *items;
    int count;
    int capacity;
} ListObj;

typedef struct {
    int ref_count;
    double *data;
    int count;
    int capacity;
} DenseListObj;

typedef struct MapEntry MapEntry;

typedef struct {
    int ref_count;
    MapEntry *entries;
    int count;
    int capacity;
} MapObj;

typedef struct {
    int ref_count;
    struct AstNode *funcdef;
    struct Env *env;
    int owns_env;
} ClosureObj;

typedef struct {
    int ref_count;
    const char *name;
    const char **fields;
    int field_count;
    int is_template;
} DataTypeObj;

#define VALUE_BLOC_INLINE_MAX 64

typedef struct {
    uint64_t handle;
} BlocValue;

typedef struct {
    uint64_t handle;
} BoxValue;

// Represents a runtime value in the language
struct Value {
    ValueType type;
    union {
        long long i;   
        double f;       
        char c;         
        int b;
        uintptr_t ptr;
        BlocTypeDesc *bloc_type;
        BlocValue bloc;
        BoxValue box;
        void *native;
        FILE *file; // Standard C File Pointer
        StringObj *string;
        ListObj *list;
        DenseListObj *dlist;
        MapObj *map;
        ClosureObj *closure;
        DataTypeObj *dtype;
        TemplateObj *template_obj;
        struct AstNode *func; // AST Pointer for user-defined functions
    };
};

struct MapEntry {
    const char *key;
    Value value;
    int occupied;
};

typedef Value (*NativeFunc)(int argc, Value *argv, struct Env *env);

// Constructors
Value value_int(long long x); 
Value value_float(double x);
Value value_string(const char *s);
Value value_string_len(const char *s, size_t len);
Value value_string_concat_raw(const char *left, size_t left_len, const char *right, size_t right_len);
Value value_string_repeat_raw(const char *s, size_t len, size_t count);
Value value_char(char c); 
Value value_bool(int b);
Value value_pointer(uintptr_t ptr);
Value value_bloc_type(const char *name, const char **fields, int field_count);
int value_bloc_check_construct(Value descriptor, int argc, Value *argv, char *msg, size_t msg_len);
Value value_bloc_construct(Value descriptor, int argc, Value *argv);
Value value_bloc_get_field(Value bloc, const char *field, int *found);
const char *value_bloc_name(Value v);
int value_bloc_equal(Value left, Value right);
Value value_box(size_t size, char *msg, size_t msg_len);
int value_box_free(Value box, char *msg, size_t msg_len);
size_t value_box_len(Value box);
size_t value_box_cap(Value box);
int value_box_is_live(Value box);
void value_box_mark_scope(Value box, uint64_t scope_id);
void value_box_release_scope(uint64_t scope_id);
void value_box_promote_to_template(Value box);
Value value_list(void);
Value value_dense_list(void); // Constructor for dense arrays
Value value_map(void);
Value value_closure(struct AstNode *funcdef, struct Env *env, int owns_env);
Value value_data_type(const char *name, const char **fields, int field_count, int is_template);
Value value_template_from_dtype(Value dtype, int argc, Value *argv, char *msg, size_t msg_len);
Value value_template_get_field(Value template_value, const char *field, int *found);
Value *value_template_field_slot(Value *template_value, const char *field);
int value_template_set_field(Value *template_value, const char *field, Value *rhs, char *msg, size_t msg_len);
const char *value_template_name(Value v);
int value_template_len(Value v);
Value value_native(NativeFunc fn); 
Value value_file(FILE *f);
Value value_null(void);

// Ref-count slow path for heap types (string, list, dense_list)
void _value_free_refcount(Value v);
Value _value_copy_refcount(Value v);

// Frees a value. No-op for primitives, decrements ref count for heap types.
static inline void value_free(Value v) {
    if (VALUE_IS_HEAP(v)) _value_free_refcount(v);
}

// Copies a value. Plain struct copy for primitives, ref count bump for heap types.
static inline Value value_copy(Value v) {
    if (!VALUE_IS_HEAP(v)) return v;
    return _value_copy_refcount(v);
}

// Transfers ownership of a value. Nulls out the source to prevent double-free.
static inline Value value_move(Value *v) {
    Value tmp = *v;
    v->type = VAL_NULL;
    return tmp;
}

// Utils
char *value_to_string(Value v);
void value_fprint(FILE *f, Value v); // Zero-alloc print directly to file stream
void value_list_append(Value *list, Value v); 
void value_list_append_move(Value *list, Value *v); // Move variant, takes ownership
void value_dlist_append(Value *list, double v); // Append to dense list
void value_map_set(Value *map, const char *key, Value v);
void value_map_set_move(Value *map, const char *key, Value *v);
Value *value_map_get(Value *map, const char *key);
int value_map_has(Value *map, const char *key);
int value_map_delete(Value *map, const char *key);
Value value_map_keys(Value map);
Value value_map_values(Value map);
Value value_map_items(Value map);
void value_gc_mark(Value *value, void *ctx);

#endif