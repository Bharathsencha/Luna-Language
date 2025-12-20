// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "token.h"
#include "mystr.h"

// Returns the character at the current position without advancing
static int lx_at(Lexer *L) {
    if (L->pos < L->len) {
        return (unsigned char)L->src[L->pos];
    }
    return 0;
}

// Returns the character at an offset from the current position
static int lx_peek(Lexer *L, int off) {
    size_t p = L->pos + off;
    if (p < L->len) {
        return (unsigned char)L->src[p];
    }
    return 0;
}

// Advances the current position by one
static void lx_advance(Lexer *L) {
    if (lx_at(L) == '\n') {
        L->line++;
        L->line_start = L->pos + 1;
    }
    if (L->pos < L->len) {
        L->pos++;
    }
    L->col = L->pos - L->line_start + 1;
}

// Skips whitespace but treats newlines as tokens (for line counting/statement end)
static void lx_skip_ws_but_keep_nl(Lexer *L) {
    while (1) {
        int c = lx_at(L);
        
        // Handle Hash comments (# ...)
        if (c == '#') {
            while (lx_at(L) != '\n' && lx_at(L) != 0) {
                lx_advance(L);
            }
            continue;
        }
        
        // Handle C-style comments (// ...)
        if (c == '/' && lx_peek(L, 1) == '/') {
            while (lx_at(L) != '\n' && lx_at(L) != 0) {
                lx_advance(L);
            }
            continue;
        }
        
        // Skip standard whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            lx_advance(L);
            continue;
        }
        break;
    }
}

// Helper to create a token struct
static Token make_token(TokenType ttype, const char *start, size_t length) {
    Token t;
    t.type = ttype;
    t.number = 0;
    t.fnumber = 0.0;
    t.line = 0; 
    t.col = 0;
    
    if (length > 0) {
        char *buf = malloc(length + 1);
        if (buf) {
            memcpy(buf, start, length);
            buf[length] = '\0';
            t.lexeme = buf;
        } else {
            t.lexeme = NULL;
        }
    } else {
        t.lexeme = my_strdup("");
    }
    return t;
}

void free_token(Token *t) {
    if (!t) {
        return;
    }
    if (t->lexeme) {
        free(t->lexeme);
    }
    t->lexeme = NULL;
}

Lexer lexer_create(const char *source) {
    Lexer L;
    L.src = source;
    L.pos = 0;
    L.len = strlen(source);
    L.line = 1;
    L.col = 1;
    L.line_start = 0;
    return L;
}

Token lexer_next(Lexer *L) {
    lx_skip_ws_but_keep_nl(L);
    int c = lx_at(L);
    int token_line = L->line;
    int token_col = L->col;
    
    if (c == 0) {
        Token t = make_token(T_EOF, "", 0);
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle Newlines
    if (c == '\n') {
        lx_advance(L);
        Token t = make_token(T_NEWLINE, "\\n", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle Strings (Double Quotes) with Escape Sequence processing
    if (c == '"') {
        lx_advance(L); // Skip opening quote
        
        // Buffer to build processed string. Max length is remaining source length.
        char *buf = malloc(L->len - L->pos + 1);
        int i = 0;

        while (lx_at(L) != 0 && lx_at(L) != '"') {
            if (lx_at(L) == '\\') {
                lx_advance(L); // Skip backslash
                int next = lx_at(L);
                if (next == 0) break;

                switch (next) {
                    case 'n':  buf[i++] = '\n'; break;
                    case 't':  buf[i++] = '\t'; break;
                    case 'r':  buf[i++] = '\r'; break;
                    case '"':  buf[i++] = '"';  break;
                    case '\\': buf[i++] = '\\'; break;
                    default:   buf[i++] = (char)next; break; 
                }
            } else {
                buf[i++] = (char)lx_at(L);
            }
            lx_advance(L);
        }

        if (lx_at(L) == '"') lx_advance(L); // Eat closing quote
        buf[i] = '\0';
        
        Token t;
        t.type = T_STRING;
        t.lexeme = buf;
        t.number = 0;
        t.fnumber = 0.0;
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle Characters (Single Quotes)
    if (c == '\'') {
        lx_advance(L); // Skip opening '
        int char_val = lx_at(L);
        
        // Simple escape handling
        if (char_val == '\\') {
            lx_advance(L);
            int esc = lx_at(L);
            if (esc == 'n') char_val = '\n';
            else if (esc == 't') char_val = '\t';
            else if (esc == '0') char_val = '\0';
            else if (esc == '\'') char_val = '\'';
            else char_val = esc;
        }
        
        lx_advance(L); // Eat the char
        if (lx_at(L) == '\'') {
            lx_advance(L); // Eat closing '
        }

        Token t;
        t.type = T_CHAR;
        t.lexeme = malloc(2);
        t.lexeme[0] = (char)char_val;
        t.lexeme[1] = '\0';
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle Multi-character Operators
    if (c == '=' && lx_peek(L, 1) == '=') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_EQEQ, "==", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }
    if (c == '!' && lx_peek(L, 1) == '=') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_NEQ, "!=", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }
    if (c == '<' && lx_peek(L, 1) == '=') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_LTE, "<=", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }
    if (c == '>' && lx_peek(L, 1) == '=') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_GTE, ">=", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }
    
    // Handle ++
    if (c == '+' && lx_peek(L, 1) == '+') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_INC, "++", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle --
    if (c == '-' && lx_peek(L, 1) == '-') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_DEC, "--", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle &&
    if (c == '&' && lx_peek(L, 1) == '&') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_AND, "&&", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle ||
    if (c == '|' && lx_peek(L, 1) == '|') {
        lx_advance(L);
        lx_advance(L);
        Token t = make_token(T_OR, "||", 2);
        t.line = token_line;
        t.col = token_col;
        return t;
    }
    // Handle Single-character Operators
    TokenType single_op = T_INVALID;
    char single_char[2] = {(char)c, '\0'};

    if (c == '=') single_op = T_EQ;
    else if (c == '+') single_op = T_PLUS;
    else if (c == '-') single_op = T_MINUS;
    else if (c == '*') single_op = T_MUL;
    else if (c == '/') single_op = T_DIV;
    else if (c == '%') single_op = T_MOD;
    else if (c == '<') single_op = T_LT;
    else if (c == '>') single_op = T_GT;
    else if (c == '(') single_op = T_LPAREN;
    else if (c == ')') single_op = T_RPAREN;
    else if (c == '{') single_op = T_LBRACE;
    else if (c == '}') single_op = T_RBRACE;
    else if (c == '[') single_op = T_LBRACKET;
    else if (c == ']') single_op = T_RBRACKET;
    else if (c == ',') single_op = T_COMMA;
    else if (c == ':') single_op = T_COLON;
    else if (c == ';') single_op = T_SEMICOLON;
    else if (c == '!') single_op = T_NOT; 
    if (single_op != T_INVALID) {
        lx_advance(L);
        Token t = make_token(single_op, single_char, 1);
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle Numbers (Integers and Floats)
    if (isdigit(c)) {
        size_t start = L->pos;
        while (isdigit(lx_at(L))) {
            lx_advance(L);
        }

        // Check for decimal point for floating point numbers
        int is_float = 0;
        if (lx_at(L) == '.' && isdigit(lx_peek(L, 1))) {
            is_float = 1;
            lx_advance(L); // eat dot
            while (isdigit(lx_at(L))) {
                lx_advance(L);
            }
        }

        size_t len = L->pos - start;
        Token t = make_token(is_float ? T_FLOAT : T_NUMBER, L->src + start, len);
        if (is_float) {
            t.fnumber = atof(t.lexeme);
        } else {
            // Use strtoll for long long
            t.number = strtoll(t.lexeme, NULL, 10);
        }
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle Identifiers and Keywords
    if (isalpha(c) || c == '_') {
        size_t start = L->pos;
        while (isalnum(lx_at(L)) || lx_at(L) == '_') {
            lx_advance(L);
        }
        size_t len = L->pos - start;
        char *buf = malloc(len + 1);
        memcpy(buf, L->src + start, len);
        buf[len] = '\0';

        TokenType tt = T_IDENT;
        // Standard Keywords
        if (!strcmp(buf, "let")) tt = T_LET;
        else if (!strcmp(buf, "if")) tt = T_IF;
        else if (!strcmp(buf, "else")) tt = T_ELSE;
        else if (!strcmp(buf, "func")) tt = T_FUNC;
        else if (!strcmp(buf, "return")) tt = T_RETURN;
        else if (!strcmp(buf, "print")) tt = T_PRINT;
        else if (!strcmp(buf, "input")) tt = T_INPUT;
        else if (!strcmp(buf, "true")) tt = T_TRUE;   
        else if (!strcmp(buf, "false")) tt = T_FALSE; 
        else if (!strcmp(buf, "while")) tt = T_WHILE;
        else if (!strcmp(buf, "for")) tt = T_FOR;
        else if (!strcmp(buf, "in")) tt = T_IN;
        else if (!strcmp(buf, "break")) tt = T_BREAK;
        else if (!strcmp(buf, "continue")) tt = T_CONTINUE;
        else if (!strcmp(buf, "switch")) tt = T_SWITCH;
        else if (!strcmp(buf, "case")) tt = T_CASE;
        else if (!strcmp(buf, "default")) tt = T_DEFAULT;

        else if (!strcmp(buf, "and")) tt = T_AND;
        else if (!strcmp(buf, "or"))  tt = T_OR;
        else if (!strcmp(buf, "not")) tt = T_NOT;
        // THE BALLS EXTENSION
        else if (!strcmp(buf, "balls")) tt = T_LET;
        else if (!strcmp(buf, "big_balls")) tt = T_LET;
        else if (!strcmp(buf, "shared_balls")) tt = T_LET;
        else if (!strcmp(buf, "loop_your_balls")) tt = T_FOR;
        else if (!strcmp(buf, "spin_balls")) tt = T_WHILE;
        else if (!strcmp(buf, "if_balls")) tt = T_IF;
        else if (!strcmp(buf, "else_balls")) tt = T_ELSE;
        else if (!strcmp(buf, "switch_balls")) tt = T_SWITCH;
        else if (!strcmp(buf, "drop_balls")) tt = T_BREAK;
        else if (!strcmp(buf, "jiggle_balls")) tt = T_CONTINUE;
        else if (!strcmp(buf, "grab_balls")) tt = T_FUNC;

        Token t;
        t.type = tt;
        t.lexeme = buf;
        t.number = 0;
        t.fnumber = 0.0;
        t.line = token_line;
        t.col = token_col;
        return t;
    }

    // Handle Unknown Characters
    size_t start = L->pos;
    lx_advance(L);
    Token t = make_token(T_IDENT, L->src + start, 1);
    t.line = token_line;
    t.col = token_col;
    return t;
}