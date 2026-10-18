// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include "token.h"

/* Markers the lexer stores for \{ and \} inside string literals. */
#define LUNA_LITERAL_LBRACE '\x01'
#define LUNA_LITERAL_RBRACE '\x02'

typedef struct {
    const char *src;
    size_t pos;
    size_t len;
    int line;
    int col;
    int line_start; // Position where current line started
} Lexer;

Lexer lexer_create(const char *source);
Token lexer_next(Lexer *L);
void free_token(Token *t);

#endif
