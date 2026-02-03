// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef VALUE_H
#define VALUE_H
#include <stdio.h> 
#include <stdbool.h>

typedef struct Value Value; // Forward decl

// Memory Management Structs
typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjList ObjList;

typedef enum {
    OBJ_STRING,
    OBJ_LIST
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked;
    struct Obj *next;
};

struct ObjString {
    Obj obj;
    char *chars;
};

struct ObjList {
    Obj obj;
    Value *items;
    int count;
    int capacity;
};

// Typedef for Native Functions
typedef Value (*NativeFunc)(int argc, Value *argv);

typedef enum {
    VAL_INT,
    VAL_FLOAT,   
    VAL_BOOL,
    VAL_CHAR,   
    VAL_NULL,   
    VAL_OBJ,
    VAL_NATIVE, 
    VAL_FILE,   // File Handle Type
} ValueType;

// Represents a runtime value in the language
struct Value {
    ValueType type;
    union {
        long long i;   
        double f;       
        int b;
        char c;         
        Obj *obj;   // The pointer to heap-allocated data
        NativeFunc native; 
        FILE *file; 
    };
};

// Constructors
Value value_int(long long x); 
Value value_float(double x);
Value value_string(const char *s);
Value value_char(char c); 
Value value_bool(int b);
Value value_list(void);
Value value_native(NativeFunc fn); 
Value value_file(FILE *f); // For file_lib
Value value_null(void);

// Utils for memory management
void value_free(Value v);
Value value_copy(Value v);
char *value_to_string(Value v);
void value_list_append(Value *list, Value v); 

// Helper Macros
#define AS_OBJ(v)     ((v).obj)
#define AS_STRING(v)  ((ObjString*)AS_OBJ(v))
#define AS_LIST(v)    ((ObjList*)AS_OBJ(v))
#define IS_OBJ(v)     ((v).type == VAL_OBJ)

#endif