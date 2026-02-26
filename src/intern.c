// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intern.h"
#include "mystr.h"

// Intern table settings. Must be power of 2 for fast & indexing
#define INTERN_CAPACITY 8192

typedef struct {
    const char **strings;
    int count;
    int capacity;
} InternTable;

// The one true global intern table for the Luna engine
static InternTable global_intern_table = {NULL, 0, 0};

// DJB2 Hash (same algorithm used in env.c for consistency)
static unsigned int intern_hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

void intern_init(void) {
    global_intern_table.capacity = INTERN_CAPACITY;
    global_intern_table.count = 0;
    // calloc sets all pointers to NULL initially
    global_intern_table.strings = calloc(global_intern_table.capacity, sizeof(const char *));
}

// Core O(1) String to Memory Resolution function
const char *intern_string(const char *str) {
    if (!str) return NULL;
    
    // Safety check - shouldn't happen if engine is initialized properly
    if (!global_intern_table.strings) {
        intern_init();
    }

    unsigned int h = intern_hash(str) & (global_intern_table.capacity - 1);
    unsigned int start_index = h;

    // Open addressing with linear probing
    while (global_intern_table.strings[h] != NULL) {
        // EXACT POINTER MATCH (It was already interned elsewhere)
        if (global_intern_table.strings[h] == str) return str;
        
        // ACTUAL STRING MATCH (Found identical characters)
        if (strcmp(global_intern_table.strings[h], str) == 0) {
            return global_intern_table.strings[h];
        }
        
        // Probe next bucket
        h = (h + 1) & (global_intern_table.capacity - 1);
        
        // Table is fully saturated
        if (h == start_index) {
            // Note: A true dynamic array would resize here, but 8192 unique strings
            // in a single script is massively overkill for the initial implementation.
            fprintf(stderr, "Fatal Error: String Intern Table Capacity Exceeded (%d)\n", global_intern_table.capacity);
            exit(1); 
        }
    }

    // String does not exist - allocate a permanent copy
    char *new_str = my_strdup(str);
    global_intern_table.strings[h] = new_str;
    global_intern_table.count++;
    
    return new_str;
}

void intern_free_all(void) {
    if (!global_intern_table.strings) return;
    
    for (int i = 0; i < global_intern_table.capacity; i++) {
        if (global_intern_table.strings[i]) {
            free((void *)global_intern_table.strings[i]);
        }
    }
    
    free(global_intern_table.strings);
    global_intern_table.strings = NULL;
    global_intern_table.count = 0;
    global_intern_table.capacity = 0;
}
