// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef VALUE_H
#define VALUE_H
#include <stdio.h> 

typedef struct Value Value; // Forward decl

// Forward declaration for Environment to avoid circular dependencies
struct Env;

// Typedef for Native Functions - Updated to support Environment access for Variable Binding
typedef Value (*NativeFunc)(int argc, Value *argv, struct Env *env);

typedef enum {
    VAL_INT,
    VAL_FLOAT,   
    VAL_STRING,
    VAL_CHAR,   
    VAL_BOOL,
    VAL_LIST,
    VAL_DENSE_LIST, // Added for high-performance SIMD/Matrix math
    VAL_NATIVE, 
    VAL_FILE,   // File Handle Type
    VAL_NULL
} ValueType;

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

// Represents a runtime value in the language
struct Value {
    ValueType type;
    union {
        long long i;   
        double f;       
        char c;         
        int b;
        NativeFunc native; 
        FILE *file; // Standard C File Pointer
        StringObj *string;
        ListObj *list;
        DenseListObj *dlist;
    };
};

// Constructors
Value value_int(long long x); 
Value value_float(double x);
Value value_string(const char *s);
Value value_char(char c); 
Value value_bool(int b);
Value value_list(void);
Value value_dense_list(void); // Constructor for dense arrays
Value value_native(NativeFunc fn); 
Value value_file(FILE *f);
Value value_null(void);

// Utils for memory management
void value_free(Value v);
Value value_copy(Value v);
char *value_to_string(Value v);
void value_list_append(Value *list, Value v); 
void value_dlist_append(Value *list, double v); // Append to dense list

#endif