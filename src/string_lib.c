// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "string_lib.h"
#include "mystr.h" // For my_strdup
#include "luna_error.h"

// Helpers
static int check_args(int argc, int expected, const char *name) {
    if (argc != expected) {
        fprintf(stderr, "Runtime Error: %s() takes %d arguments.\n", name, expected);
        return 0;
    }
    return 1;
}

// Helper to ensure an argument is a string and return it
static const char *get_str_arg(Value *argv, int index) {
    //  Check for Object type and String subtype
    if (!IS_OBJ(argv[index]) || AS_OBJ(argv[index])->type != OBJ_STRING) return NULL;
    return AS_STRING(argv[index])->chars;
}

// Basic Operations
// consolidated len() implementation in string_lib.c or a general lib file
Value lib_len(int argc, Value *argv) {
    if (argc != 1) {
        error_report(ERR_ARGUMENT, 0, 0, "len() expects exactly 1 argument", "Usage: len(variable)");
        return value_null();
    }

    Value v = argv[0];
    //  Updated to use IS_OBJ and subtype checks
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) {
        return value_int((long long)strlen(AS_STRING(v)->chars));
    } 
    else if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_LIST) {
        return value_int((long long)AS_LIST(v)->count);
    } 
    else {
        error_report(ERR_TYPE, 0, 0, "len() cannot be used on this type", 
                     "len() works on strings and lists.");
        return value_null();
    }
}

Value lib_str_len(int argc, Value *argv) {
    if (argc != 1) {
        error_report(ERR_ARGUMENT, 0, 0, "len() expects exactly 1 argument", "Usage: len(variable)");
        return value_null();
    }

    Value v = argv[0];
    //  Updated to use IS_OBJ and subtype checks
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) {
        return value_int((long long)strlen(AS_STRING(v)->chars));
    } 
    else if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_LIST) {
        return value_int((long long)AS_LIST(v)->count);
    } 
    else {
        error_report(ERR_TYPE, 0, 0, "len() cannot be used on this type", 
                     "len() works on strings and lists.");
        return value_null();
    }
}

Value lib_str_is_empty(int argc, Value *argv) {
    if (!check_args(argc, 1, "is_empty")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_bool(1);
    return value_bool(strlen(s) == 0);
}

Value lib_str_concat(int argc, Value *argv) {
    if (!check_args(argc, 2, "concat")) return value_null();
    
    // Convert both to strings if they aren't already
    char *s1 = value_to_string(argv[0]);
    char *s2 = value_to_string(argv[1]);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    char *res = malloc(len1 + len2 + 1);
    strcpy(res, s1);
    strcat(res, s2);
    
    free(s1);
    free(s2);
    
    Value v = value_string(res);
    free(res); // value_string makes a copy
    return v;
}

// Slicing

Value lib_str_substring(int argc, Value *argv) {
    if (!check_args(argc, 3, "substring")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    long long start = argv[1].i;
    long long len = argv[2].i;
    long long str_len = strlen(s);
    
    if (start < 0) start = 0;
    if (start >= str_len) return value_string("");
    if (len < 0) len = 0;
    if (start + len > str_len) len = str_len - start;
    
    char *sub = malloc(len + 1);
    strncpy(sub, s + start, len);
    sub[len] = '\0';
    
    Value v = value_string(sub);
    free(sub);
    return v;
}

Value lib_str_slice(int argc, Value *argv) {
    if (!check_args(argc, 3, "slice")) return value_null();

    // Add List support for slicing
    // Updated check for List Object
    if (IS_OBJ(argv[0]) && AS_OBJ(argv[0])->type == OBJ_LIST) {
        Value src_val = argv[0];
        ObjList *src = AS_LIST(src_val); // Cast to ObjList
        
        long long start = argv[1].i;
        long long end = argv[2].i;
        long long count = src->count;

        // Handle negative indices
        if (start < 0) start += count;
        if (end < 0) end += count;

        // Clamp values
        if (start < 0) start = 0;
        if (end > count) end = count;
        if (start >= end) return value_list();

        Value result = value_list();
        for (long long i = start; i < end; i++) {
            value_list_append(&result, src->items[i]);
        }
        return result;
    }

    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    long long start = argv[1].i;
    long long end = argv[2].i;
    long long str_len = strlen(s);
    
    // Handle negative indices (Python style)
    if (start < 0) start += str_len;
    if (end < 0) end += str_len;
    
    // Clamp
    // I still down understand how tf this works
    if (start < 0) start = 0;
    if (end > str_len) end = str_len;
    if (start >= end) return value_string("");
    
    long long len = end - start;
    char *sub = malloc(len + 1);
    strncpy(sub, s + start, len);
    sub[len] = '\0';
    
    Value v = value_string(sub);
    free(sub);
    return v;
}

Value lib_str_char_at(int argc, Value *argv) {
    if (!check_args(argc, 2, "char_at")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    long long idx = argv[1].i;
    if (idx < 0 || (size_t)idx >= strlen(s)) return value_string("");
    
    // Return as a single-char string
    char tmp[2] = { s[idx], '\0' };
    return value_string(tmp);
}

//  Searching 
Value lib_str_index_of(int argc, Value *argv) {
    if (!check_args(argc, 2, "index_of")) return value_null();
    const char *haystack = get_str_arg(argv, 0);
    const char *needle = get_str_arg(argv, 1);
    if (!haystack || !needle) return value_int(-1);
    
    char *found = strstr(haystack, needle);
    if (!found) return value_int(-1);
    
    return value_int((long long)(found - haystack));
}

Value lib_str_last_index_of(int argc, Value *argv) {
    if (!check_args(argc, 2, "last_index_of")) return value_null();
    const char *haystack = get_str_arg(argv, 0);
    const char *needle = get_str_arg(argv, 1);
    if (!haystack || !needle) return value_int(-1);
    
    size_t nlen = strlen(needle);
    size_t hlen = strlen(haystack);
    
    if (nlen > hlen) return value_int(-1);
    
    // Search backwards
    for (long long i = hlen - nlen; i >= 0; i--) {
        if (strncmp(haystack + i, needle, nlen) == 0) {
            return value_int(i);
        }
    }
    return value_int(-1);
}

Value lib_str_contains(int argc, Value *argv) {
    Value idx = lib_str_index_of(argc, argv);
    int found = (idx.i != -1);
    value_free(idx); // Just checking existence
    return value_bool(found);
}

Value lib_str_starts_with(int argc, Value *argv) {
    if (!check_args(argc, 2, "starts_with")) return value_null();
    const char *s = get_str_arg(argv, 0);
    const char *pre = get_str_arg(argv, 1);
    if (!s || !pre) return value_bool(0);
    
    size_t slen = strlen(s);
    size_t plen = strlen(pre);
    if (plen > slen) return value_bool(0);
    
    return value_bool(strncmp(s, pre, plen) == 0);
}

Value lib_str_ends_with(int argc, Value *argv) {
    if (!check_args(argc, 2, "ends_with")) return value_null();
    const char *s = get_str_arg(argv, 0);
    const char *suf = get_str_arg(argv, 1);
    if (!s || !suf) return value_bool(0);
    
    size_t slen = strlen(s);
    size_t ulen = strlen(suf);
    if (ulen > slen) return value_bool(0);
    
    return value_bool(strcmp(s + slen - ulen, suf) == 0);
}

// Transformations 
Value lib_str_to_upper(int argc, Value *argv) {
    if (!check_args(argc, 1, "to_upper")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    char *dup = my_strdup(s);
    for (int i = 0; dup[i]; i++) {
        dup[i] = toupper((unsigned char)dup[i]);
    }
    Value v = value_string(dup);
    free(dup);
    return v;
}

Value lib_str_to_lower(int argc, Value *argv) {
    if (!check_args(argc, 1, "to_lower")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    char *dup = my_strdup(s);
    for (int i = 0; dup[i]; i++) {
        dup[i] = tolower((unsigned char)dup[i]);
    }
    Value v = value_string(dup);
    free(dup);
    return v;
}

Value lib_str_trim(int argc, Value *argv) {
    if (!check_args(argc, 1, "trim")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return value_string("");
    
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    
    size_t len = end - s + 1;
    char *res = malloc(len + 1);
    memcpy(res, s, len);
    res[len] = '\0';
    
    Value v = value_string(res);
    free(res);
    return v;
}

Value lib_str_trim_left(int argc, Value *argv) {
    if (!check_args(argc, 1, "trim_left")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    while (isspace((unsigned char)*s)) s++;
    return value_string(s);
}

Value lib_str_trim_right(int argc, Value *argv) {
    if (!check_args(argc, 1, "trim_right")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();
    
    size_t len = strlen(s);
    if (len == 0) return value_string("");
    
    const char *end = s + len - 1;
    while (end >= s && isspace((unsigned char)*end)) end--;
    
    size_t new_len = end - s + 1;
    char *res = malloc(new_len + 1);
    memcpy(res, s, new_len);
    res[new_len] = '\0';
    
    Value v = value_string(res);
    free(res);
    return v;
}

Value lib_str_replace(int argc, Value *argv) {
    if (!check_args(argc, 3, "replace")) return value_null();
    const char *s = get_str_arg(argv, 0);
    const char *old = get_str_arg(argv, 1);
    const char *new = get_str_arg(argv, 2);
    
    if (!s || !old || !new) return value_null();
    
    size_t old_len = strlen(old);
    size_t new_len = strlen(new);
    if (old_len == 0) return value_string(s); // Prevent infinite loop
    
    // Count occurrences
    int count = 0;
    const char *tmp = s;
    while ((tmp = strstr(tmp, old))) {
        count++;
        tmp += old_len;
    }
    
    size_t result_len = strlen(s) + count * (new_len - old_len);
    char *result = malloc(result_len + 1);
    char *ptr = result;
    
    while (*s) {
        if (strncmp(s, old, old_len) == 0) {
            strcpy(ptr, new);
            ptr += new_len;
            s += old_len;
        } else {
            *ptr++ = *s++;
        }
    }
    *ptr = '\0';
    
    Value v = value_string(result);
    free(result);
    return v;
}

Value lib_str_reverse(int argc, Value *argv) {
    if (!check_args(argc, 1, "reverse")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_null();

    size_t len = strlen(s);
    char *rev = malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        rev[i] = s[len - 1 - i];
    }
    rev[len] = '\0';
    Value v = value_string(rev);
    free(rev);
    return v;
}

Value lib_str_repeat(int argc, Value *argv) {
    if (!check_args(argc, 2, "repeat")) return value_null();
    const char *s = get_str_arg(argv, 0);
    long long count = argv[1].i;

    if (!s || count <= 0) return value_string("");

    size_t len = strlen(s);
    char *res = malloc(len * count + 1);
    res[0] = '\0';

    for (int i = 0; i < count; i++) {
        strcat(res, s); // Slightly inefficient but safe. memcpy is faster.
    }
    Value v = value_string(res);
    free(res);
    return v;
}

Value lib_str_pad_left(int argc, Value *argv) {
    if (!check_args(argc, 3, "pad_left")) return value_null();
    const char *s = get_str_arg(argv, 0);
    long long width = argv[1].i;
    // Third arg is CHAR string
    const char *pad_char_str = get_str_arg(argv, 2);
    char pad_c = (pad_char_str && strlen(pad_char_str) > 0) ? pad_char_str[0] : ' ';

    if (!s) return value_null();
    size_t len = strlen(s);
    if ((long long)len >= width) return value_string(s);

    long long pad_len = width - len;
    char *res = malloc(width + 1);
    
    // Fill padding
    memset(res, pad_c, pad_len);
    // Copy string
    strcpy(res + pad_len, s);
    
    Value v = value_string(res);
    free(res);
    return v;
}

Value lib_str_pad_right(int argc, Value *argv) {
    if (!check_args(argc, 3, "pad_right")) return value_null();
    const char *s = get_str_arg(argv, 0);
    long long width = argv[1].i;
    const char *pad_char_str = get_str_arg(argv, 2);
    char pad_c = (pad_char_str && strlen(pad_char_str) > 0) ? pad_char_str[0] : ' ';

    if (!s) return value_null();
    size_t len = strlen(s);
    if ((long long)len >= width) return value_string(s);

    char *res = malloc(width + 1);
    strcpy(res, s);
    // Fill rest
    long long pad_len = width - len;
    memset(res + len, pad_c, pad_len);
    res[width] = '\0';

    Value v = value_string(res);
    free(res);
    return v;
}

// Lists (Split/Join)
Value lib_str_split(int argc, Value *argv) {
    if (!check_args(argc, 2, "split")) return value_null();
    const char *s = get_str_arg(argv, 0);
    const char *delim = get_str_arg(argv, 1);
    
    if (!s || !delim) return value_list();
    
    Value list = value_list();
    if (strlen(delim) == 0) {
        // If empty delimiter, split into chars
        for (size_t i = 0; i < strlen(s); i++) {
            char tmp[2] = { s[i], '\0' };
            value_list_append(&list, value_string(tmp));
        }
        return list;
    }
    
    char *copy = my_strdup(s);
    char *token = strtok(copy, delim);
    while (token) {
        value_list_append(&list, value_string(token));
        token = strtok(NULL, delim);
    }
    free(copy);
    return list;
}

Value lib_str_join(int argc, Value *argv) {
    if (!check_args(argc, 2, "join")) return value_null();
    
    // Arg 0 is LIST, Arg 1 is Delimiter
    // Updated check for List Object
    if (!IS_OBJ(argv[0]) || AS_OBJ(argv[0])->type != OBJ_LIST) return value_string("");
    
    const char *delim = get_str_arg(argv, 1);
    if (!delim) delim = "";
    
    // Calculate total length
    size_t total_len = 0;
    size_t delim_len = strlen(delim);
    
    ObjList *list = AS_LIST(argv[0]); // Cast to ObjList
    int count = list->count;
    
    for (int i = 0; i < count; i++) {
        char *s = value_to_string(list->items[i]);
        total_len += strlen(s);
        free(s);
        if (i < count - 1) total_len += delim_len;
    }
    
    char *res = malloc(total_len + 1);
    res[0] = '\0';
    
    for (int i = 0; i < count; i++) {
        char *s = value_to_string(list->items[i]);
        strcat(res, s);
        free(s);
        if (i < count - 1) strcat(res, delim);
    }
    
    Value v = value_string(res);
    free(res);
    return v;
}

// Character Checks
Value lib_str_is_digit(int argc, Value *argv) {
    if (!check_args(argc, 1, "is_digit")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s || strlen(s) == 0) return value_bool(0);
    
    for (int i = 0; s[i]; i++) {
        if (!isdigit((unsigned char)s[i])) return value_bool(0);
    }
    return value_bool(1);
}

Value lib_str_is_alpha(int argc, Value *argv) {
    if (!check_args(argc, 1, "is_alpha")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s || strlen(s) == 0) return value_bool(0);
    
    for (int i = 0; s[i]; i++) {
        if (!isalpha((unsigned char)s[i])) return value_bool(0);
    }
    return value_bool(1);
}

Value lib_str_is_alnum(int argc, Value *argv) {
    if (!check_args(argc, 1, "is_alnum")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s || strlen(s) == 0) return value_bool(0);
    
    for (int i = 0; s[i]; i++) {
        if (!isalnum((unsigned char)s[i])) return value_bool(0);
    }
    return value_bool(1);
}

Value lib_str_is_space(int argc, Value *argv) {
    if (!check_args(argc, 1, "is_space")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s || strlen(s) == 0) return value_bool(0);
    
    for (int i = 0; s[i]; i++) {
        if (!isspace((unsigned char)s[i])) return value_bool(0);
    }
    return value_bool(1);
}

// Type Conversions
Value lib_str_to_int(int argc, Value *argv) {
    if (!check_args(argc, 1, "to_int")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_int(0);
    return value_int(strtoll(s, NULL, 10));
}

Value lib_str_to_float(int argc, Value *argv) {
    if (!check_args(argc, 1, "to_float")) return value_null();
    const char *s = get_str_arg(argv, 0);
    if (!s) return value_float(0.0);
    return value_float(atof(s));
}