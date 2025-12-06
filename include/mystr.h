// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef MYSTR_H
#define MYSTR_H

#include <stdlib.h>
#include <string.h>

// POSIX strdup is not standard C, so we define our own helper
static inline char *my_strdup(const char *s) {
    size_t len = strlen(s);
    char *p = malloc(len + 1);
    if (p) {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}

#endif