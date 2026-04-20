// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    T_EOF = 0,
    T_IDENT,
    T_NUMBER, // Integers (long long)
    T_FLOAT,  // Floating point numbers
    T_STRING,
    T_CHAR,   // Character literals 'a'
    T_TRUE,
    T_FALSE,

    // Logical Operators
    T_AND,      // && or 'and'
    T_OR,       // || or 'or'
    T_NOT,      // !  or 'not'

    // Operators
    T_PLUS,
    T_PLUS_EQ,
    T_INC,      // ++ 
    T_DEC,      // --
    T_MINUS, 
    T_MINUS_EQ,
    T_MUL, 
    T_MUL_EQ,
    T_DIV, 
    T_DIV_EQ,
    T_MOD,
    T_EQ,       // = (Assignment)
    T_EQEQ,     // ==
    T_NEQ,      // !=
    T_LT, 
    T_GT, 
    T_LTE, 
    T_GTE,

    // Punctuation
    T_LPAREN, 
    T_RPAREN, 
    T_LBRACE, 
    T_RBRACE,
    T_LBRACKET, 
    T_RBRACKET, //[ ]
    T_COMMA,
    T_DOT,
    T_SEMICOLON,
    T_NEWLINE,

    // Keywords
    T_LET, 
    T_IF, 
    T_ELSE, 
    T_FUNC, 
    T_RETURN,
    T_PRINT, 
    T_INPUT, 
    T_WHILE, 
    T_BREAK, 
    T_CONTINUE, 
    T_SWITCH, 
    T_CASE, 
    T_DEFAULT, 
    T_COLON,
    T_FOR, 
    T_IN,
    T_CONST,
    T_DATA,
    T_TEMPLATE,
    T_BLOC,
    T_BOX,
    T_USE,
    T_FROM,
    T_EXPORT,
    T_IMPORT,
    T_UNSAFE,

    T_INVALID
} TokenType;

#define TOKEN_INLINE_MAX 24

typedef struct {
    TokenType type;
    char *lexeme;                  // heap-allocated, or NULL if inline
    char ibuf[TOKEN_INLINE_MAX];   // inline buffer for short lexemes
    long long number;
    double fnumber;
    int line;
    int col;
} Token;

// Get the lexeme string — works for both inline and heap tokens
static inline const char *token_str(const Token *t) {
    return t->lexeme ? t->lexeme : t->ibuf;
}

// Returns the string representation of a token type
const char *token_name(TokenType t);

#endif