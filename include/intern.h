// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef INTERN_H
#define INTERN_H

// Initializes the global string interning hash set
void intern_init(void);

// Interns a string, returning a guaranteed unique pointer for its contents.
// If the string already exists, returns the existing pointer.
// If it does not exist, copies the string into the intern table and returns the new pointer.
const char *intern_string(const char *str);

// Frees all strings in the intern table and the table itself
void intern_free_all(void);

#endif
