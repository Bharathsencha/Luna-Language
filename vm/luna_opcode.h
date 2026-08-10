// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_OPCODE_H
#define LUNA_OPCODE_H

typedef enum {
    VM_OP_HALT = 0,
    VM_OP_LOAD_INT,       // VM_OP_LOAD_INT dst_reg, int_val_64bit
    VM_OP_LOAD_FLOAT,     // VM_OP_LOAD_FLOAT dst_reg, double_val_64bit
    VM_OP_LOAD_CONST,     // VM_OP_LOAD_CONST dst_reg, const_idx_16bit
    VM_OP_LOAD_TRUE,      // VM_OP_LOAD_TRUE dst_reg
    VM_OP_LOAD_FALSE,     // VM_OP_LOAD_FALSE dst_reg
    VM_OP_LOAD_NULL,      // VM_OP_LOAD_NULL dst_reg
    VM_OP_MOVE,           // VM_OP_MOVE dst_reg, src_reg

    // Binary operations
    VM_OP_ADD,            // VM_OP_ADD dst_reg, lhs_reg, rhs_reg
    VM_OP_SUB,            // VM_OP_SUB dst_reg, lhs_reg, rhs_reg
    VM_OP_MUL,            // VM_OP_MUL dst_reg, lhs_reg, rhs_reg
    VM_OP_DIV,            // VM_OP_DIV dst_reg, lhs_reg, rhs_reg
    VM_OP_MOD,            // VM_OP_MOD dst_reg, lhs_reg, rhs_reg

    // Comparison operations
    VM_OP_EQ,             // VM_OP_EQ dst_reg, lhs_reg, rhs_reg
    VM_OP_NEQ,            // VM_OP_NEQ dst_reg, lhs_reg, rhs_reg
    VM_OP_LT,             // VM_OP_LT dst_reg, lhs_reg, rhs_reg
    VM_OP_LTE,            // VM_OP_LTE dst_reg, lhs_reg, rhs_reg
    VM_OP_GT,             // VM_OP_GT dst_reg, lhs_reg, rhs_reg
    VM_OP_GTE,            // VM_OP_GTE dst_reg, lhs_reg, rhs_reg

    // Unary operations
    VM_OP_NOT,            // VM_OP_NOT dst_reg, src_reg
    VM_OP_NEG,            // VM_OP_NEG dst_reg, src_reg

    // Control flow (16-bit signed offsets)
    VM_OP_JUMP,           // VM_OP_JUMP offset_16bit
    VM_OP_JUMP_IF_TRUE,   // VM_OP_JUMP_IF_TRUE cond_reg, offset_16bit
    VM_OP_JUMP_IF_FALSE,  // VM_OP_JUMP_IF_FALSE cond_reg, offset_16bit

    // Global variables
    VM_OP_GET_GLOBAL,     // VM_OP_GET_GLOBAL dst_reg, name_const_idx_16bit
    VM_OP_SET_GLOBAL,     // VM_OP_SET_GLOBAL name_const_idx_16bit, src_reg

    // Local / upvalues
    VM_OP_GET_UPVAL,      // VM_OP_GET_UPVAL dst_reg, upval_idx_8bit
    VM_OP_SET_UPVAL,      // VM_OP_SET_UPVAL upval_idx_8bit, src_reg

    // Collection ops
    VM_OP_NEW_LIST,       // VM_OP_NEW_LIST dst_reg
    VM_OP_LIST_APPEND,    // VM_OP_LIST_APPEND list_reg, val_reg
    VM_OP_INDEX_GET,      // VM_OP_INDEX_GET dst_reg, target_reg, idx_reg
    VM_OP_INDEX_SET,      // VM_OP_INDEX_SET target_reg, idx_reg, val_reg

    // Fields / Properties
    VM_OP_FIELD_GET,      // VM_OP_FIELD_GET dst_reg, target_reg, name_const_idx_16bit
    VM_OP_FIELD_SET,      // VM_OP_FIELD_SET target_reg, name_const_idx_16bit, val_reg

    // Functions
    VM_OP_CALL,           // VM_OP_CALL dst_reg, func_reg, argc_8bit
    VM_OP_RETURN,         // VM_OP_RETURN val_reg
    VM_OP_CLOSURE,        // VM_OP_CLOSURE dst_reg, subchunk_idx_16bit

    VM_OP_PRINT,          // VM_OP_PRINT src_reg
    VM_OP_SAFEPOINT       // VM_OP_SAFEPOINT
} Opcode;

#endif // LUNA_OPCODE_H
