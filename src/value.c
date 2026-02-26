// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"
#include "mystr.h"

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
    v.type = VAL_STRING;
    v.string = malloc(sizeof(StringObj));
    v.string->ref_count = 1;
    if (s) {
        v.string->chars = my_strdup(s);
    } else {
        v.string->chars = my_strdup("");
    }
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
    v.type = VAL_LIST;
    v.list = malloc(sizeof(ListObj));
    v.list->ref_count = 1;
    v.list->items = NULL;
    v.list->count = 0;
    v.list->capacity = 0;
    return v;
}

// Constructor for empty dense lists
Value value_dense_list(void) {
    Value v;
    v.type = VAL_DENSE_LIST;
    v.dlist = malloc(sizeof(DenseListObj));
    v.dlist->ref_count = 1;
    v.dlist->data = NULL;
    v.dlist->count = 0;
    v.dlist->capacity = 0;
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

// Frees memory associated with a Value (deep free for lists)
void value_free(Value v) {
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
            // Note: v.dlist->data is now allocated on the AST Arena which handles bulk cleanup.
            // Do NOT call free() here, or it will segfault.
            free(v.dlist);
        }
    }
}

// Creates a deep copy of a Value
Value value_copy(Value v) {
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
        case VAL_STRING:
            r.string = v.string;
            if (r.string) r.string->ref_count++;
            break;
        case VAL_LIST:
            r.list = v.list;
            if (r.list) r.list->ref_count++;
            break;
        case VAL_DENSE_LIST:
            r.dlist = v.dlist;
            if (r.dlist) r.dlist->ref_count++;
            break;
        default:
            r.i = 0;
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
        case VAL_STRING:
            if (v.string && v.string->chars) {
                return my_strdup(v.string->chars);
            } else {
                return my_strdup("");
            }
        case VAL_LIST: {
            char *res = my_strdup("[");
            if (v.list) {
                for (int i = 0; i < v.list->count; i++) {
                    char *vs = value_to_string(v.list->items[i]);
                    size_t new_len = strlen(res) + strlen(vs) + 3;
                    res = realloc(res, new_len);
                    strcat(res, vs);
                    if (i < v.list->count - 1) {
                        strcat(res, ", ");
                    }
                    free(vs);
                }
            }
            res = realloc(res, strlen(res) + 2);
            strcat(res, "]");
            return res;
        }
        case VAL_DENSE_LIST: {
            char *res = my_strdup("d[");
            if (v.dlist) {
                for (int i = 0; i < v.dlist->count; i++) {
                    char tbuf[64];
                    snprintf(tbuf, 64, "%.6g", v.dlist->data[i]);
                    size_t new_len = strlen(res) + strlen(tbuf) + 3;
                    res = realloc(res, new_len);
                    strcat(res, tbuf);
                    if (i < v.dlist->count - 1) {
                        strcat(res, ", ");
                    }
                }
            }
            res = realloc(res, strlen(res) + 2);
            strcat(res, "]");
            return res;
        }
        default:
            return my_strdup("null");
    }
}

// Appends a value to a list, resizing capacity if needed
void value_list_append(Value *list, Value v) {
    if (list->type != VAL_LIST || !list->list) {
        return;
    }
    if (list->list->count >= list->list->capacity) {
        int n = list->list->capacity == 0 ? 4 : list->list->capacity * 2;
        list->list->items = realloc(list->list->items, sizeof(Value) * n);
        list->list->capacity = n;
    }
    list->list->items[list->list->count++] = value_copy(v);
}

// Appends a double directly to a dense list
void value_dlist_append(Value *list, double v) {
    if (list->type != VAL_DENSE_LIST || !list->dlist) {
        return;
    }
    if (list->dlist->count >= list->dlist->capacity) {
        int n = list->dlist->capacity == 0 ? 4 : list->dlist->capacity * 2;
        list->dlist->data = realloc(list->dlist->data, sizeof(double) * n);
        list->dlist->capacity = n;
    }
    list->dlist->data[list->dlist->count++] = v;
}