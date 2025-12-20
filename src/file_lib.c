// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_lib.h"
#include "value.h"
#include "mystr.h"

// Helper: Safely extract the FILE pointer from a Luna Value
static FILE *get_file_ptr(Value v) {
    if (v.type == VAL_FILE) {
        return v.file;
    }
    return NULL;
}

// Helper: Standardized argument count and type checking
static int check_args(int argc, int expected, const char *name) {
    if (argc != expected) {
        fprintf(stderr, "Runtime Error: %s() takes %d arguments.\n", name, expected);
        return 0;
    }
    return 1;
}

// open(path, mode) -> returns VAL_FILE or VAL_NULL
Value lib_file_open(int argc, Value *argv) {
    if (!check_args(argc, 2, "open")) return value_null();
    
    if (argv[0].type != VAL_STRING || argv[1].type != VAL_STRING) {
        fprintf(stderr, "Runtime Error: open() expects strings for path and mode.\n");
        return value_null();
    }

    const char *path = argv[0].s;
    const char *mode = argv[1].s;

    FILE *f = fopen(path, mode);
    if (!f) return value_null();

    return value_file(f); 
}

// close(file_handle) -> returns null
Value lib_file_close(int argc, Value *argv) {
    if (!check_args(argc, 1, "close")) return value_null();

    FILE *f = get_file_ptr(argv[0]);
    if (f) {
        fclose(f);
        // Important: invalidate the handle in the current scope to prevent double-close
        argv[0].file = NULL; 
    }
    return value_null();
}

// read(file_handle) -> returns full content as a single string
Value lib_file_read(int argc, Value *argv) {
    if (!check_args(argc, 1, "read")) return value_null();

    FILE *f = get_file_ptr(argv[0]);
    if (!f) return value_null();

    // Determine file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size < 0) return value_null();

    char *buf = malloc(size + 1);
    if (!buf) return value_null();

    size_t read_size = fread(buf, 1, size, f);
    buf[read_size] = '\0';

    Value v = value_string(buf);
    free(buf);
    return v;
}

// read_line(file_handle) -> returns a string with trailing newlines removed
Value lib_file_read_line(int argc, Value *argv) {
    if (!check_args(argc, 1, "read_line")) return value_null();

    FILE *f = get_file_ptr(argv[0]);
    if (!f) return value_null();

    char buf[1024];
    if (fgets(buf, sizeof(buf), f)) {
        // Strip trailing newline characters (\n or \r) to fix assertion failures in tests
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        return value_string(buf);
    }
    return value_null(); // End of file or error
}

// write(file_handle, data) -> returns boolean success
Value lib_file_write(int argc, Value *argv) {
    if (!check_args(argc, 2, "write")) return value_null();

    FILE *f = get_file_ptr(argv[0]);
    if (!f) {
        fprintf(stderr, "Runtime Error: write() called on invalid file handle.\n");
        return value_null();
    }

    // Convert any Luna type to string before writing
    char *text = value_to_string(argv[1]);
    int res = fputs(text, f);
    free(text);

    return value_bool(res != EOF);
}

// file_exists(path) -> returns boolean
Value lib_file_exists(int argc, Value *argv) {
    if (!check_args(argc, 1, "file_exists")) return value_null();
    if (argv[0].type != VAL_STRING) return value_bool(0);

    FILE *f = fopen(argv[0].s, "r");
    if (f) {
        fclose(f);
        return value_bool(1);
    }
    return value_bool(0);
}

// remove_file(path) -> returns boolean
Value lib_file_remove(int argc, Value *argv) {
    if (!check_args(argc, 1, "remove_file")) return value_null();
    if (argv[0].type != VAL_STRING) return value_bool(0);
    
    int res = remove(argv[0].s);
    return value_bool(res == 0);
}

// flush(file_handle) -> returns null
Value lib_file_flush(int argc, Value *argv) {
    if (!check_args(argc, 1, "flush")) return value_null();

    FILE *f = get_file_ptr(argv[0]);
    if (f) {
        fflush(f);
    }
    return value_null();
}