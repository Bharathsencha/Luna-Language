// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include "token.h"

// Converts a TokenType enum value into a readable string.
// This is primarily used for debugging and generating syntax error messages.
const char *token_name(TokenType t) {
    switch (t) {
        case T_EOF:
            return "EOF";
        case T_NEWLINE:
            return "NEWLINE";
        case T_LET:
            return "LET";
        case T_IF:
            return "IF";
        case T_ELSE:
            return "ELSE";
        case T_FUNC:
            return "FUNC";
        case T_RETURN:
            return "RETURN";
        case T_PRINT:
            return "PRINT";
        case T_INPUT:
            return "INPUT";
        case T_TRUE:
            return "TRUE";
        case T_FALSE:
            return "FALSE";
        case T_IDENT:
            return "IDENT";
        case T_NUMBER:
            return "NUMBER";
        case T_FLOAT:
            return "FLOAT";
        case T_STRING:
            return "STRING";
        case T_CHAR:  
            return "CHAR";
        case T_PLUS:
            return "PLUS";
        case T_MINUS:
            return "MINUS";
        case T_MUL:
            return "MUL";
        case T_DIV:
            return "DIV";
        case T_MOD:
            return "MOD";
        case T_INC:     
            return "INC";
        case T_DEC:     
            return "DEC";
        case T_EQ:
            return "EQ";
        case T_EQEQ:
            return "EQEQ";
        case T_NEQ:
            return "NEQ";
        case T_LT:
            return "LT";
        case T_GT:
            return "GT";
        case T_LTE:
            return "LTE";
        case T_GTE:
            return "GTE";
        case T_LPAREN:
            return "LPAREN";
        case T_RPAREN:
            return "RPAREN";
        case T_LBRACE:
            return "LBRACE";
        case T_RBRACE:
            return "RBRACE";
        case T_LBRACKET:
            return "LBRACKET";
        case T_RBRACKET:
            return "RBRACKET";
        case T_COMMA:
            return "COMMA";
        case T_COLON:
            return "COLON";
        case T_SEMICOLON: 
            return "SEMICOLON"; 
        case T_WHILE:
            return "WHILE";
        case T_FOR:
            return "FOR";
        case T_IN:
            return "IN";
        case T_BREAK:
            return "BREAK";
        case T_CONTINUE:
            return "CONTINUE";
        case T_SWITCH:
            return "SWITCH";
        case T_CASE:
            return "CASE";
        case T_DEFAULT:
            return "DEFAULT";
        default:
            return "UNKNOWN";
    }
}