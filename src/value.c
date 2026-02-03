// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"
#include "mystr.h"
#include "gc.h" 

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

// Constructor for string values
Value value_string(const char *s) {
    Value v;
    v.type = VAL_OBJ; // Changed to VAL_OBJ for GC
    
    ObjString *str = (ObjString*)gc_allocate(sizeof(ObjString), OBJ_STRING);
    if (s) {
        str->chars = my_strdup(s);
    } else {
        str->chars = my_strdup("");
    }
    
    v.obj = (Obj*)str;
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

// Constructor for empty lists
Value value_list(void) {
    Value v;
    v.type = VAL_OBJ; // Changed to VAL_OBJ for GC
    
    ObjList *list = (ObjList*)gc_allocate(sizeof(ObjList), OBJ_LIST);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    
    v.obj = (Obj*)list;
    return v;
}

// Constructor for native functions
Value value_native(NativeFunc fn) {
    Value v;
    v.type = VAL_NATIVE;
    v.native = fn;
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

// Frees memory associated with a Value (Updated: Objects are now managed by GC)
void value_free(Value v) {
    // With reference semantics, we DO NOT free objects here.
    // They are cleaned up by the Garbage Collector.
    if (v.type == VAL_OBJ) {
        return; 
    }
    // VAL_NATIVE does not need freeing (function pointer is static/global)
    // VAL_FILE does not need freeing here (files must be closed explicitly via close())
}

// Creates a copy of a Value (Updated: Shallow copy for Objects)
Value value_copy(Value v) {
    // For objects, we only copy the pointer (Reference Semantics)
    if (v.type == VAL_OBJ) {
        return v;
    }
    
    Value r;
    r.type = v.type;
    switch (v.type) {
        case VAL_INT:
            r.i = v.i;
            break;
        case VAL_FLOAT:
            r.f = v.f;
            break;
        case VAL_BOOL:
            r.b = v.b;
            break;
        case VAL_CHAR:
            r.c = v.c;
            break;
        case VAL_NATIVE:
            r.native = v.native;
            break;
        case VAL_FILE:
            r.file = v.file;
            break;
        default:
            r.i = 0; // NULL
            break;
    }
    return r;
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
        case VAL_CHAR:
            snprintf(buf, 128, "%c", v.c);
            return my_strdup(buf);
        case VAL_NATIVE:
            return my_strdup("<native function>");
        case VAL_FILE:
            if (v.file) return my_strdup("<file handle>");
            else return my_strdup("<closed file>");
            
        case VAL_OBJ: {
            Obj *obj = AS_OBJ(v);
            if (obj->type == OBJ_STRING) {
                return my_strdup(AS_STRING(v)->chars);
            } 
            else if (obj->type == OBJ_LIST) {
                ObjList *list = AS_LIST(v);
                char *res = my_strdup("[");
                for (int i = 0; i < list->count; i++) {
                    char *vs = value_to_string(list->items[i]);
                    size_t new_len = strlen(res) + strlen(vs) + 3;
                    res = realloc(res, new_len);
                    strcat(res, vs);
                    if (i < list->count - 1) {
                        strcat(res, ", ");
                    }
                    free(vs);
                }
                res = realloc(res, strlen(res) + 2);
                strcat(res, "]");
                return res;
            }
        }
        default:
            return my_strdup("null");
    }
}

// Appends a value to a list, resizing capacity if needed
void value_list_append(Value *list_val, Value v) {
    if (!IS_OBJ(*list_val) || AS_OBJ(*list_val)->type != OBJ_LIST) {
        return;
    }
    
    ObjList *list = AS_LIST(*list_val);
    
    if (list->count >= list->capacity) {
        int n = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Value) * n);
        list->capacity = n;
    }
    list->items[list->count++] = value_copy(v);
}