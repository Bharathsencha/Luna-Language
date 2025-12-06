// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include "util.h"

// Reads the entire content of a file into a dynamically allocated string.
// Returns NULL if the file cannot be opened or read.
char *read_file(const char *path) {
    // Open file in binary mode to read exact bytes
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    // Seek to the end of the file to determine its size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET); // Rewind to the beginning

    // Allocate memory for file content plus a null terminator
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    // Read the file content into the buffer
    if (fread(buf, 1, size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }

    // Null-terminate the buffer to make it a valid C string
    buf[size] = '\0';
    
    fclose(f);
    return buf;
}