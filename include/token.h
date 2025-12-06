// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

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

    // Operators
    T_PLUS,
    T_INC,      // ++ 
    T_DEC,      // --
    T_MINUS, 
    T_MUL, 
    T_DIV, 
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

    T_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    long long number;
    double fnumber;
    int line;
    int col;
} Token;

// Returns the string representation of a token type
const char *token_name(TokenType t);

#endif