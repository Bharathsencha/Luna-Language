// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef VALUE_H
#define VALUE_H

typedef struct Value Value; // Forward decl

// Typedef for Native Functions
typedef Value (*NativeFunc)(int argc, Value *argv);

typedef enum {
    VAL_INT,
    VAL_FLOAT,   
    VAL_STRING,
    VAL_CHAR,   // Character type
    VAL_BOOL,
    VAL_LIST,
    VAL_NATIVE, // New type for Native C Functions
    VAL_NULL
} ValueType;

// Represents a runtime value in the language
struct Value {
    ValueType type;
    union {
        long long i;   
        double f;       
        char *s;
        char c;         // char storage
        int b;
        NativeFunc native; // Storage for function pointer
        struct {        //List
            struct Value *items;
            int count;
            int capacity;
        } list;
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
Value value_null(void);

// Utils for memory management
void value_free(Value v);
Value value_copy(Value v);
char *value_to_string(Value v);
void value_list_append(Value *list, Value v); 

#endif