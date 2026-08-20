// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "luna_compiler.h"
#include "luna_opcode.h"
#include "intern.h"

typedef struct {
    const char *name;
    int depth;
    int reg;
    int is_upvalue;
} Local;

typedef struct {
    uint8_t index;
    int is_local;
} Upvalue;

typedef struct Loop {
    int start_ip;
    int scope_depth;
    int *break_jumps;
    int break_count;
    int break_cap;
    int is_switch; /* pseudo-loop for switch: `break` ends switch, `continue` skips it */
    struct Loop *outer;
} Loop;

typedef struct Compiler {
    struct Compiler *parent;
    LunaChunk *chunk;
    
    Local locals[512];
    int local_count;
    int scope_depth;
    int next_reg;
    int max_regs;

    Upvalue upvalues[256];
    int upvalue_count;

    Loop *current_loop;
    int last_line;
    int stmt_count;
} Compiler;

static void compiler_init(Compiler *c, Compiler *parent, const char *name) {
    c->parent = parent;
    c->chunk = malloc(sizeof(LunaChunk));
    luna_chunk_init(c->chunk);
    c->chunk->name = name ? name : "<main>";
    c->local_count = 0;
    c->scope_depth = 0;
    c->next_reg = 0;
    c->max_regs = 0;
    c->upvalue_count = 0;
    c->current_loop = NULL;
    c->last_line = 1;
    c->stmt_count = 0;

    // For function compilation, parameter registers are allocated starting from 0.
}

static int allocate_reg(Compiler *c) {
    int reg = c->next_reg++;
    if (c->next_reg > c->max_regs) {
        c->max_regs = c->next_reg;
    }
    return reg;
}

static void emit_byte(Compiler *c, uint8_t byte, int line) {
    luna_chunk_write(c->chunk, byte, line);
    c->last_line = line;
}

static void emit_opcode(Compiler *c, Opcode op, int line) {
    emit_byte(c, (uint8_t)op, line);
}

static void emit_16(Compiler *c, uint16_t val, int line) {
    emit_byte(c, (uint8_t)(val & 0xFF), line);
    emit_byte(c, (uint8_t)((val >> 8) & 0xFF), line);
}

static void emit_2(Compiler *c, Opcode op, uint8_t arg1, int line) {
    emit_opcode(c, op, line);
    emit_byte(c, arg1, line);
}

static void emit_3(Compiler *c, Opcode op, uint8_t arg1, uint8_t arg2, int line) {
    emit_opcode(c, op, line);
    emit_byte(c, arg1, line);
    emit_byte(c, arg2, line);
}

static void emit_4(Compiler *c, Opcode op, uint8_t arg1, uint8_t arg2, uint8_t arg3, int line) {
    emit_opcode(c, op, line);
    emit_byte(c, arg1, line);
    emit_byte(c, arg2, line);
    emit_byte(c, arg3, line);
}

static int emit_jump(Compiler *c, Opcode op, int line) {
    emit_opcode(c, op, line);
    int jump_ip = (int)c->chunk->code_len;
    emit_16(c, 0xFFFF, line);
    return jump_ip;
}

static int emit_jump_cond(Compiler *c, Opcode op, uint8_t cond_reg, int line) {
    emit_opcode(c, op, line);
    emit_byte(c, cond_reg, line);
    int jump_ip = (int)c->chunk->code_len;
    emit_16(c, 0xFFFF, line);
    return jump_ip;
}

static void patch_jump(Compiler *c, int jump_ip) {
    int offset = (int)c->chunk->code_len - (jump_ip + 2);
    if (offset < -32768 || offset > 32767) {
        fprintf(stderr, "Compile error: jump offset out of range\n");
        abort();
    }
    c->chunk->code[jump_ip] = (uint8_t)(offset & 0xFF);
    c->chunk->code[jump_ip + 1] = (uint8_t)((offset >> 8) & 0xFF);
}

static void emit_loop_jump(Compiler *c, int dest_ip, int line) {
    emit_opcode(c, VM_OP_JUMP, line);
    int offset = dest_ip - ((int)c->chunk->code_len + 2);
    if (offset < -32768 || offset > 32767) {
        fprintf(stderr, "Compile error: loop offset out of range\n");
        abort();
    }
    emit_16(c, (uint16_t)offset, line);
}

static int resolve_local(Compiler *c, const char *name) {
    for (int i = c->local_count - 1; i >= 0; i--) {
        if (c->locals[i].name == name) {
            return c->locals[i].reg;
        }
    }
    return -1;
}

static int add_upvalue(Compiler *c, uint8_t index, int is_local) {
    for (int i = 0; i < c->upvalue_count; i++) {
        if (c->upvalues[i].index == index && c->upvalues[i].is_local == is_local) {
            return i;
        }
    }
    c->upvalues[c->upvalue_count].index = index;
    c->upvalues[c->upvalue_count].is_local = is_local;
    return c->upvalue_count++;
}

static int resolve_upvalue(Compiler *c, const char *name) {
    if (c->parent == NULL) return -1;

    int local = resolve_local(c->parent, name);
    if (local != -1) {
        c->parent->locals[local].is_upvalue = 1;
        return add_upvalue(c, (uint8_t)local, 1);
    }

    int upvalue = resolve_upvalue(c->parent, name);
    if (upvalue != -1) {
        return add_upvalue(c, (uint8_t)upvalue, 0);
    }

    return -1;
}

static void add_local(Compiler *c, const char *name, int line) {
    if (c->local_count >= 512) {
        fprintf(stderr, "Compile error: too many local variables\n");
        abort();
    }
    int reg = c->local_count;
    c->locals[c->local_count].name = name;
    c->locals[c->local_count].depth = c->scope_depth;
    c->locals[c->local_count].reg = reg;
    c->locals[c->local_count].is_upvalue = 0;
    c->local_count++;
    c->next_reg = c->local_count;
    if (reg >= c->max_regs) {
        c->max_regs = reg + 1;
    }
}

static void begin_scope(Compiler *c, int line) {
    c->scope_depth++;
    emit_opcode(c, VM_OP_SCOPE_BEGIN, line);
}

static void end_scope(Compiler *c, int line) {
    c->scope_depth--;
    emit_opcode(c, VM_OP_SCOPE_EXIT, line);
    // Pop locals from scope
    while (c->local_count > 0 && c->locals[c->local_count - 1].depth > c->scope_depth) {
        c->local_count--;
    }
    c->next_reg = c->local_count;
}

static int compile_expr(Compiler *c, AstNode *n, int target_reg);
static void compile_stmt(Compiler *c, AstNode *n);

static int compile_expr_to_any_reg(Compiler *c, AstNode *n) {
    return compile_expr(c, n, -1);
}

/* Compiles a function definition into a subchunk, emits VM_OP_CLOSURE with
 * upvalue capture data, and returns the register holding the closure. */
static int compile_function_value(Compiler *c, AstNode *n, int line) {
    Compiler fn_compiler;
    compiler_init(&fn_compiler, c, n->funcdef.name);
    fn_compiler.chunk->param_count = n->funcdef.param_count;

    // Parameter variables are mapped to registers 0, 1, 2...
    for (int i = 0; i < n->funcdef.param_count; i++) {
        add_local(&fn_compiler, n->funcdef.params[i], line);
    }

    // Default parameter values: fill in registers for missing args.
    for (int i = 0; i < n->funcdef.param_count; i++) {
        if (!n->funcdef.defaults || !n->funcdef.defaults[i]) continue;
        int has = allocate_reg(&fn_compiler);
        emit_2(&fn_compiler, VM_OP_HAS_ARG, has, line);
        emit_byte(&fn_compiler, (uint8_t)i, line);
        int skip = emit_jump_cond(&fn_compiler, VM_OP_JUMP_IF_TRUE, has, line);
        compile_expr(&fn_compiler, n->funcdef.defaults[i], i);
        patch_jump(&fn_compiler, skip);
    }
    fn_compiler.next_reg = fn_compiler.local_count;

    // Function body scope: box lifetime + deferred calls are scoped here.
    emit_opcode(&fn_compiler, VM_OP_SCOPE_BEGIN, line);
    for (int i = 0; i < n->funcdef.body.count; i++) {
        compile_stmt(&fn_compiler, n->funcdef.body.items[i]);
    }
    emit_opcode(&fn_compiler, VM_OP_SCOPE_EXIT, line);

    // Implicit return null if function reaches end
    int null_reg = allocate_reg(&fn_compiler);
    emit_2(&fn_compiler, VM_OP_LOAD_NULL, null_reg, line);
    emit_2(&fn_compiler, VM_OP_RETURN, null_reg, line);

    fn_compiler.chunk->reg_count = fn_compiler.max_regs;
    fn_compiler.chunk->upvalue_count = fn_compiler.upvalue_count;

    // Add function subchunk
    int sub_idx = luna_chunk_add_subchunk(c->chunk, fn_compiler.chunk);

    // Instantiate closure at runtime
    int dst = allocate_reg(c);
    emit_2(c, VM_OP_CLOSURE, dst, line);
    emit_16(c, sub_idx, line);

    // Write upvalue data so VM knows how to capture them
    for (int i = 0; i < fn_compiler.upvalue_count; i++) {
        emit_byte(c, fn_compiler.upvalues[i].is_local ? 1 : 0, line);
        emit_byte(c, fn_compiler.upvalues[i].index, line);
    }

    return dst;
}

static int compile_expr(Compiler *c, AstNode *n, int target_reg) {
    if (!n) return -1;
    int line = n->line;
    int old_reg = c->next_reg;

    switch (n->kind) {
        case NODE_NUMBER: {
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_opcode(c, VM_OP_LOAD_INT, line);
            emit_byte(c, dst, line);
            // Write 64-bit int value directly into bytecode
            long long val = n->number.value;
            for (int i = 0; i < 8; i++) {
                emit_byte(c, (uint8_t)((val >> (i * 8)) & 0xFF), line);
            }
            return dst;
        }
        case NODE_FLOAT: {
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_opcode(c, VM_OP_LOAD_FLOAT, line);
            emit_byte(c, dst, line);
            double val = n->fnumber.value;
            uint64_t bits;
            memcpy(&bits, &val, sizeof(double));
            for (int i = 0; i < 8; i++) {
                emit_byte(c, (uint8_t)((bits >> (i * 8)) & 0xFF), line);
            }
            return dst;
        }
        case NODE_STRING: {
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            Value v = value_string(n->string.text);
            int const_idx = luna_chunk_add_constant(c->chunk, v);
            emit_2(c, VM_OP_LOAD_CONST, dst, line);
            emit_16(c, const_idx, line);
            return dst;
        }
        case NODE_CHAR: {
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            Value v = value_char(n->character.value);
            int const_idx = luna_chunk_add_constant(c->chunk, v);
            emit_2(c, VM_OP_LOAD_CONST, dst, line);
            emit_16(c, const_idx, line);
            return dst;
        }
        case NODE_BOOL: {
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_2(c, n->boolean.value ? VM_OP_LOAD_TRUE : VM_OP_LOAD_FALSE, dst, line);
            return dst;
        }
        case NODE_IDENT: {
            // Local
            int reg = resolve_local(c, n->ident.name);
            if (reg != -1) {
                if (target_reg != -1 && target_reg != reg) {
                    emit_3(c, VM_OP_MOVE, target_reg, reg, line);
                    return target_reg;
                }
                return reg;
            }
            // Upvalue
            int upval = resolve_upvalue(c, n->ident.name);
            if (upval != -1) {
                int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
                emit_3(c, VM_OP_GET_UPVAL, dst, (uint8_t)upval, line);
                return dst;
            }
            // Global
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            Value name_val = value_string(n->ident.name);
            int name_idx = luna_chunk_add_constant(c->chunk, name_val);
            emit_2(c, VM_OP_GET_GLOBAL, dst, line);
            emit_16(c, name_idx, line);
            return dst;
        }
        case NODE_BINOP: {
            /* Logical AND/OR with short-circuit semantics */
            if (n->binop.op == OP_AND || n->binop.op == OP_OR) {
                int lhs = compile_expr_to_any_reg(c, n->binop.left);
                c->next_reg = old_reg;
                int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
                emit_3(c, VM_OP_MOVE, dst, lhs, line);
                int skip = emit_jump_cond(c,
                    (n->binop.op == OP_AND) ? VM_OP_JUMP_IF_FALSE : VM_OP_JUMP_IF_TRUE,
                    dst, line);
                compile_expr(c, n->binop.right, dst);
                patch_jump(c, skip);
                c->next_reg = old_reg;
                return dst;
            }

            int lhs = compile_expr_to_any_reg(c, n->binop.left);
            int rhs = compile_expr_to_any_reg(c, n->binop.right);
            
            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            
            Opcode op = VM_OP_ADD;
            switch (n->binop.op) {
                case OP_ADD: op = VM_OP_ADD; break;
                case OP_SUB: op = VM_OP_SUB; break;
                case OP_MUL: op = VM_OP_MUL; break;
                case OP_DIV: op = VM_OP_DIV; break;
                case OP_MOD: op = VM_OP_MOD; break;
                case OP_EQ:  op = VM_OP_EQ; break;
                case OP_NEQ: op = VM_OP_NEQ; break;
                case OP_LT:  op = VM_OP_LT; break;
                case OP_LTE: op = VM_OP_LTE; break;
                case OP_GT:  op = VM_OP_GT; break;
                case OP_GTE: op = VM_OP_GTE; break;
                default:
                    fprintf(stderr, "Compile error: unsupported binop %d\n", n->binop.op);
                    abort();
            }
            emit_4(c, op, dst, lhs, rhs, line);
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_NOT: {
            int src = compile_expr_to_any_reg(c, n->logic_not.expr);
            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_3(c, VM_OP_NOT, dst, src, line);
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_INC: {
            int reg = resolve_local(c, n->inc.name);
            if (reg != -1) {
                int temp = allocate_reg(c);
                emit_opcode(c, VM_OP_LOAD_INT, line);
                emit_byte(c, temp, line);
                long long one = 1;
                for (int i = 0; i < 8; i++) emit_byte(c, (uint8_t)((one >> (i * 8)) & 0xFF), line);
                emit_4(c, VM_OP_ADD, reg, reg, temp, line);
                c->next_reg = old_reg;
                if (target_reg != -1) {
                    emit_3(c, VM_OP_MOVE, target_reg, reg, line);
                    return target_reg;
                }
                return reg;
            }
            // Global
            int temp_val = allocate_reg(c);
            Value name_val = value_string(n->inc.name);
            int name_idx = luna_chunk_add_constant(c->chunk, name_val);
            emit_2(c, VM_OP_GET_GLOBAL, temp_val, line);
            emit_16(c, name_idx, line);

            int temp_one = allocate_reg(c);
            emit_opcode(c, VM_OP_LOAD_INT, line);
            emit_byte(c, temp_one, line);
            long long one = 1;
            for (int i = 0; i < 8; i++) emit_byte(c, (uint8_t)((one >> (i * 8)) & 0xFF), line);

            emit_4(c, VM_OP_ADD, temp_val, temp_val, temp_one, line);
            emit_opcode(c, VM_OP_SET_GLOBAL, line);
            emit_16(c, name_idx, line);
            emit_byte(c, temp_val, line);

            c->next_reg = old_reg;
            if (target_reg != -1) {
                emit_3(c, VM_OP_MOVE, target_reg, temp_val, line);
                return target_reg;
            }
            int dst = allocate_reg(c);
            emit_3(c, VM_OP_MOVE, dst, temp_val, line);
            return dst;
        }
        case NODE_DEC: {
            int reg = resolve_local(c, n->dec.name);
            if (reg != -1) {
                int temp = allocate_reg(c);
                emit_opcode(c, VM_OP_LOAD_INT, line);
                emit_byte(c, temp, line);
                long long one = 1;
                for (int i = 0; i < 8; i++) emit_byte(c, (uint8_t)((one >> (i * 8)) & 0xFF), line);
                emit_4(c, VM_OP_SUB, reg, reg, temp, line);
                c->next_reg = old_reg;
                if (target_reg != -1) {
                    emit_3(c, VM_OP_MOVE, target_reg, reg, line);
                    return target_reg;
                }
                return reg;
            }
            // Global
            int temp_val = allocate_reg(c);
            Value name_val = value_string(n->dec.name);
            int name_idx = luna_chunk_add_constant(c->chunk, name_val);
            emit_2(c, VM_OP_GET_GLOBAL, temp_val, line);
            emit_16(c, name_idx, line);

            int temp_one = allocate_reg(c);
            emit_opcode(c, VM_OP_LOAD_INT, line);
            emit_byte(c, temp_one, line);
            long long one = 1;
            for (int i = 0; i < 8; i++) emit_byte(c, (uint8_t)((one >> (i * 8)) & 0xFF), line);

            emit_4(c, VM_OP_SUB, temp_val, temp_val, temp_one, line);
            emit_opcode(c, VM_OP_SET_GLOBAL, line);
            emit_16(c, name_idx, line);
            emit_byte(c, temp_val, line);

            c->next_reg = old_reg;
            if (target_reg != -1) {
                emit_3(c, VM_OP_MOVE, target_reg, temp_val, line);
                return target_reg;
            }
            int dst = allocate_reg(c);
            emit_3(c, VM_OP_MOVE, dst, temp_val, line);
            return dst;
        }
        case NODE_LIST: {
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_2(c, VM_OP_NEW_LIST, dst, line);
            for (int i = 0; i < n->list.items.count; i++) {
                int item_reg = compile_expr_to_any_reg(c, n->list.items.items[i]);
                emit_3(c, VM_OP_LIST_APPEND, dst, item_reg, line);
                c->next_reg = old_reg + 1; // keep dst
            }
            c->next_reg = old_reg;
            if (target_reg == -1) dst = allocate_reg(c);
            return dst;
        }
        case NODE_INDEX: {
            int target = compile_expr_to_any_reg(c, n->index.target);
            int index = compile_expr_to_any_reg(c, n->index.index);
            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_4(c, VM_OP_INDEX_GET, dst, target, index, line);
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_FIELD: {
            int target = compile_expr_to_any_reg(c, n->field.target);
            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            Value name_val = value_string(n->field.field);
            int name_idx = luna_chunk_add_constant(c->chunk, name_val);
            emit_2(c, VM_OP_FIELD_GET, dst, line);
            emit_byte(c, target, line);
            emit_16(c, name_idx, line);
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_MAP: {
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_2(c, VM_OP_NEW_MAP, dst, line);
            for (int i = 0; i < n->map.count; i++) {
                int val = compile_expr_to_any_reg(c, n->map.values[i]);
                Value key_val = value_string(n->map.keys[i]);
                int key_idx = luna_chunk_add_constant(c->chunk, key_val);
                emit_2(c, VM_OP_MAP_SET, dst, line);
                emit_16(c, key_idx, line);
                emit_byte(c, (uint8_t)val, line);
                c->next_reg = old_reg + 1; /* keep dst alive */
            }
            c->next_reg = old_reg;
            return dst;
        }
        case NODE_TEMPLATE: {
            /* String interpolation compiled as a chain of string concats. */
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            Value head = value_string(n->template_string.chunks[0] ?
                                       n->template_string.chunks[0] : "");
            int head_idx = luna_chunk_add_constant(c->chunk, head);
            emit_2(c, VM_OP_LOAD_CONST, dst, line);
            emit_16(c, head_idx, line);
            for (int i = 0; i < n->template_string.expr_count; i++) {
                int part = compile_expr_to_any_reg(c, n->template_string.exprs[i]);
                emit_4(c, VM_OP_ADD, dst, dst, part, line);
                const char *chunk = n->template_string.chunks[i + 1] ?
                                    n->template_string.chunks[i + 1] : "";
                Value cv = value_string(chunk);
                int c_idx = luna_chunk_add_constant(c->chunk, cv);
                int c_reg = allocate_reg(c);
                emit_2(c, VM_OP_LOAD_CONST, c_reg, line);
                emit_16(c, c_idx, line);
                emit_4(c, VM_OP_ADD, dst, dst, c_reg, line);
                c->next_reg = old_reg + 1; /* keep dst alive */
            }
            c->next_reg = old_reg;
            return dst;
        }
        case NODE_TYPED_INIT: {
            int callee = allocate_reg(c);
            Value name_val = value_string(n->typed_init.name);
            int name_idx = luna_chunk_add_constant(c->chunk, name_val);
            emit_2(c, VM_OP_GET_GLOBAL, callee, line);
            emit_16(c, name_idx, line);
            for (int i = 0; i < n->typed_init.args.count; i++) {
                compile_expr(c, n->typed_init.args.items[i], allocate_reg(c));
            }
            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_4(c, VM_OP_CALL, dst, callee, (uint8_t)n->typed_init.args.count, line);
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_BOX_ALLOC: {
            int size = compile_expr_to_any_reg(c, n->box_alloc.size);
            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_3(c, VM_OP_BOX_ALLOC, dst, size, line);
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_INPUT: {
            int callee = allocate_reg(c);
            Value name_val = value_string("input");
            int name_idx = luna_chunk_add_constant(c->chunk, name_val);
            emit_2(c, VM_OP_GET_GLOBAL, callee, line);
            emit_16(c, name_idx, line);
            int arg = allocate_reg(c);
            Value prompt = value_string(n->input.prompt ? n->input.prompt : "");
            int prompt_idx = luna_chunk_add_constant(c->chunk, prompt);
            emit_2(c, VM_OP_LOAD_CONST, arg, line);
            emit_16(c, prompt_idx, line);
            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            emit_4(c, VM_OP_CALL, dst, callee, 1, line);
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_FUNC_DEF: {
            int dst = compile_function_value(c, n, line);
            if (target_reg != -1 && target_reg != dst) {
                emit_3(c, VM_OP_MOVE, target_reg, dst, line);
                return target_reg;
            }
            return dst;
        }
        case NODE_CALL: {
            // Builtin optimizations / fast-paths (like append, len, clock)
            if (n->call.kind == CALL_APPEND && n->call.args.count == 2) {
                int list = compile_expr_to_any_reg(c, n->call.args.items[0]);
                int val = compile_expr_to_any_reg(c, n->call.args.items[1]);
                emit_3(c, VM_OP_LIST_APPEND, list, val, line);
                c->next_reg = old_reg;
                int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
                emit_2(c, VM_OP_LOAD_NULL, dst, line);
                if (target_reg == -1) {
                    c->next_reg = dst + 1;
                }
                return dst;
            }

            // address_of(x): pointer to a named value's storage.
            if (n->call.kind == CALL_ADDRESS_OF && n->call.args.count == 1) {
                AstNode *arg = n->call.args.items[0];
                if (arg && arg->kind == NODE_IDENT) {
                    int reg = resolve_local(c, arg->ident.name);
                    int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
                    if (reg != -1) {
                        emit_3(c, VM_OP_ADDR_OF, dst, (uint8_t)reg, line);
                    } else {
                        Value name_val = value_string(arg->ident.name);
                        int name_idx = luna_chunk_add_constant(c->chunk, name_val);
                        emit_2(c, VM_OP_ADDR_OF_GLOBAL, dst, line);
                        emit_16(c, name_idx, line);
                    }
                    if (target_reg == -1) {
                        c->next_reg = dst + 1;
                    }
                    return dst;
                }
            }

            // defer(call): capture the call now, run it at scope exit.
            if (n->call.kind == CALL_DEFER && n->call.args.count == 1 &&
                n->call.args.items[0]->kind == NODE_CALL) {
                int callee = allocate_reg(c);
                compile_expr(c, n->call.args.items[0]->call.callee, callee);
                int argc = n->call.args.items[0]->call.args.count;
                for (int i = 0; i < argc; i++) {
                    compile_expr(c, n->call.args.items[0]->call.args.items[i], allocate_reg(c));
                }
                c->next_reg = old_reg;
                emit_3(c, VM_OP_DEFER, callee, (uint8_t)argc, line);
                int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
                emit_2(c, VM_OP_LOAD_NULL, dst, line);
                if (target_reg == -1) {
                    c->next_reg = dst + 1;
                }
                return dst;
            }

            int callee = allocate_reg(c);
            compile_expr(c, n->call.callee, callee);
            
            // Allocate sequential registers for arguments starting right after callee
            for (int i = 0; i < n->call.args.count; i++) {
                compile_expr(c, n->call.args.items[i], allocate_reg(c));
            }

            c->next_reg = old_reg;
            int dst = (target_reg != -1) ? target_reg : allocate_reg(c);
            if (n->call.callee && n->call.callee->kind == NODE_IDENT) {
                Value name_val = value_string(n->call.callee->ident.name);
                int name_idx = luna_chunk_add_constant(c->chunk, name_val);
                emit_4(c, VM_OP_CALL_NAMED, dst, callee, (uint8_t)n->call.args.count, line);
                emit_16(c, name_idx, line);
            } else {
                emit_4(c, VM_OP_CALL, dst, callee, (uint8_t)n->call.args.count, line);
            }
            if (target_reg == -1) {
                c->next_reg = dst + 1;
            }
            return dst;
        }
        case NODE_GROUP: {
            int last_reg = -1;
            for (int i = 0; i < n->block.items.count; i++) {
                last_reg = compile_expr(c, n->block.items.items[i], target_reg);
            }
            return last_reg;
        }
        default:
            fprintf(stderr, "Compile error: unknown expression node kind %s (%d)\n", ast_node_kind_name(n->kind), n->kind);
            abort();
    }
    return -1;
}

static void compile_stmt(Compiler *c, AstNode *n) {
    if (!n) return;
    int line = n->line;
    int old_reg = c->next_reg;

    /* Inject periodic safepoints so straight-line code still lets the
     * incremental GC make progress. */
    if (++c->stmt_count % 16 == 0 && n->kind != NODE_BLOCK) {
        emit_opcode(c, VM_OP_SAFEPOINT, line);
    }

    switch (n->kind) {
        case NODE_BLOCK: {
            begin_scope(c, line);
            for (int i = 0; i < n->block.items.count; i++) {
                compile_stmt(c, n->block.items.items[i]);
            }
            end_scope(c, line);
            break;
        }
        case NODE_LET: {
            /* Top-level lets define globals in the environment so they are
             * visible to imports, the REPL, and auto-call main(). */
            if (c->parent == NULL && c->scope_depth == 0) {
                int val_reg = compile_expr_to_any_reg(c, n->let.expr);
                Value name_val = value_string(n->let.name);
                int name_idx = luna_chunk_add_constant(c->chunk, name_val);
                emit_opcode(c, VM_OP_SET_GLOBAL, line);
                emit_16(c, name_idx, line);
                emit_byte(c, val_reg, line);
                c->next_reg = old_reg;
                break;
            }
            int val_reg = compile_expr_to_any_reg(c, n->let.expr);
            // Add variable to local list
            add_local(c, n->let.name, line);
            int local_reg = c->locals[c->local_count - 1].reg;
            if (val_reg != local_reg) {
                emit_3(c, VM_OP_MOVE, local_reg, val_reg, line);
            }
            break;
        }
        case NODE_ASSIGN: {
            int reg = resolve_local(c, n->assign.name);
            if (reg != -1) {
                compile_expr(c, n->assign.expr, reg);
            } else {
                int upval = resolve_upvalue(c, n->assign.name);
                if (upval != -1) {
                    int val_reg = compile_expr_to_any_reg(c, n->assign.expr);
                    emit_3(c, VM_OP_SET_UPVAL, (uint8_t)upval, val_reg, line);
                } else {
                    int val_reg = compile_expr_to_any_reg(c, n->assign.expr);
                    Value name_val = value_string(n->assign.name);
                    int name_idx = luna_chunk_add_constant(c->chunk, name_val);
                    emit_opcode(c, VM_OP_SET_GLOBAL, line);
                    emit_16(c, name_idx, line);
                    emit_byte(c, val_reg, line);
                }
            }
            c->next_reg = old_reg;
            break;
        }
        case NODE_ASSIGN_INDEX: {
            int list = compile_expr_to_any_reg(c, n->assign_index.list);
            int index = compile_expr_to_any_reg(c, n->assign_index.index);
            int val = compile_expr_to_any_reg(c, n->assign_index.value);
            emit_4(c, VM_OP_INDEX_SET, list, index, val, line);
            c->next_reg = old_reg;
            break;
        }
        case NODE_PRINT: {
            for (int i = 0; i < n->print.args.count; i++) {
                int reg = compile_expr_to_any_reg(c, n->print.args.items[i]);
                emit_2(c, VM_OP_PRINT, reg, line);
                c->next_reg = old_reg; // free temp immediately
            }
            break;
        }
        case NODE_IF: {
            int cond = compile_expr_to_any_reg(c, n->ifstmt.cond);
            int else_jump = emit_jump_cond(c, VM_OP_JUMP_IF_FALSE, cond, line);
            c->next_reg = old_reg;

            begin_scope(c, line);
            for (int i = 0; i < n->ifstmt.then_block.count; i++) {
                compile_stmt(c, n->ifstmt.then_block.items[i]);
            }
            end_scope(c, line);

            int main_jump = emit_jump(c, VM_OP_JUMP, line);
            patch_jump(c, else_jump);

            begin_scope(c, line);
            for (int i = 0; i < n->ifstmt.else_block.count; i++) {
                compile_stmt(c, n->ifstmt.else_block.items[i]);
            }
            end_scope(c, line);

            patch_jump(c, main_jump);
            break;
        }
        case NODE_WHILE: {
            int start_ip = (int)c->chunk->code_len;
            emit_opcode(c, VM_OP_SAFEPOINT, line);

            int cond = compile_expr_to_any_reg(c, n->whilestmt.cond);
            int exit_jump = emit_jump_cond(c, VM_OP_JUMP_IF_FALSE, cond, line);
            c->next_reg = old_reg;

            Loop loop;
            loop.start_ip = start_ip;
            loop.scope_depth = c->scope_depth;
            loop.break_jumps = NULL;
            loop.break_count = 0;
            loop.break_cap = 0;
            loop.is_switch = 0;
            loop.outer = c->current_loop;
            c->current_loop = &loop;

            begin_scope(c, line);
            for (int i = 0; i < n->whilestmt.body.count; i++) {
                compile_stmt(c, n->whilestmt.body.items[i]);
            }
            end_scope(c, line);

            emit_loop_jump(c, start_ip, line);
            patch_jump(c, exit_jump);

            // Patch all break jumps
            for (int i = 0; i < loop.break_count; i++) {
                patch_jump(c, loop.break_jumps[i]);
            }
            if (loop.break_jumps) free(loop.break_jumps);

            c->current_loop = loop.outer;
            break;
        }
        case NODE_FOR: {
            begin_scope(c, line);
            // Compile Init (e.g. let i = 0)
            if (n->forstmt.init) {
                compile_stmt(c, n->forstmt.init);
            }

            int start_ip = (int)c->chunk->code_len;
            emit_opcode(c, VM_OP_SAFEPOINT, line);

            int exit_jump = -1;
            if (n->forstmt.cond) {
                int cond = compile_expr_to_any_reg(c, n->forstmt.cond);
                exit_jump = emit_jump_cond(c, VM_OP_JUMP_IF_FALSE, cond, line);
                c->next_reg = c->local_count; // preserve loop locals only
            }

            Loop loop;
            loop.start_ip = start_ip;
            loop.scope_depth = c->scope_depth;
            loop.break_jumps = NULL;
            loop.break_count = 0;
            loop.break_cap = 0;
            loop.is_switch = 0;
            loop.outer = c->current_loop;
            c->current_loop = &loop;

            // Body
            begin_scope(c, line);
            for (int i = 0; i < n->forstmt.body.count; i++) {
                compile_stmt(c, n->forstmt.body.items[i]);
            }
            end_scope(c, line);

            // Increment
            if (n->forstmt.incr) {
                compile_expr_to_any_reg(c, n->forstmt.incr);
                c->next_reg = c->local_count;
            }

            emit_loop_jump(c, start_ip, line);
            
            if (exit_jump != -1) {
                patch_jump(c, exit_jump);
            }

            // Patch breaks
            for (int i = 0; i < loop.break_count; i++) {
                patch_jump(c, loop.break_jumps[i]);
            }
            if (loop.break_jumps) free(loop.break_jumps);

            c->current_loop = loop.outer;
            end_scope(c, line); // pops loop variable
            break;
        }
        case NODE_FOR_IN: {
            begin_scope(c, line);
            /* Loop-carried registers are hidden locals so body statements can
             * never clobber them. */
            add_local(c, n->forin.name, line);
            int var_reg = c->locals[c->local_count - 1].reg;
            add_local(c, NULL, line); /* iter_reg */
            int iter_reg = c->locals[c->local_count - 1].reg;
            add_local(c, NULL, line); /* count_reg */
            int count_reg = c->locals[c->local_count - 1].reg;
            add_local(c, NULL, line); /* i_reg */
            int i_reg = c->locals[c->local_count - 1].reg;

            compile_expr(c, n->forin.iterable, iter_reg);

            /* count = len(iterable) */
            int callee = allocate_reg(c);
            Value len_val = value_string("len");
            int len_idx = luna_chunk_add_constant(c->chunk, len_val);
            emit_2(c, VM_OP_GET_GLOBAL, callee, line);
            emit_16(c, len_idx, line);
            int arg_reg = allocate_reg(c);
            emit_3(c, VM_OP_MOVE, arg_reg, iter_reg, line);
            emit_4(c, VM_OP_CALL_NAMED, count_reg, callee, 1, line);
            emit_16(c, len_idx, line);

            emit_opcode(c, VM_OP_LOAD_INT, line);
            emit_byte(c, i_reg, line);
            long long zero = 0;
            for (int b = 0; b < 8; b++) emit_byte(c, (uint8_t)((zero >> (b * 8)) & 0xFF), line);

            Loop loop;
            loop.start_ip = 0;
            loop.scope_depth = c->scope_depth;
            loop.break_jumps = NULL;
            loop.break_count = 0;
            loop.break_cap = 0;
            loop.is_switch = 0;
            loop.outer = c->current_loop;
            c->current_loop = &loop;

            int start_ip = (int)c->chunk->code_len;
            loop.start_ip = start_ip;
            emit_opcode(c, VM_OP_SAFEPOINT, line);

            int cmp = allocate_reg(c);
            emit_4(c, VM_OP_LT, cmp, i_reg, count_reg, line);
            int exit_jump = emit_jump_cond(c, VM_OP_JUMP_IF_FALSE, cmp, line);

            emit_4(c, VM_OP_INDEX_GET, var_reg, iter_reg, i_reg, line);

            for (int j = 0; j < n->forin.body.count; j++) {
                compile_stmt(c, n->forin.body.items[j]);
            }

            /* i = i + 1 */
            int one_reg = allocate_reg(c);
            emit_opcode(c, VM_OP_LOAD_INT, line);
            emit_byte(c, one_reg, line);
            long long one = 1;
            for (int b = 0; b < 8; b++) emit_byte(c, (uint8_t)((one >> (b * 8)) & 0xFF), line);
            emit_4(c, VM_OP_ADD, i_reg, i_reg, one_reg, line);

            emit_loop_jump(c, start_ip, line);
            patch_jump(c, exit_jump);

            for (int j = 0; j < loop.break_count; j++) {
                patch_jump(c, loop.break_jumps[j]);
            }
            if (loop.break_jumps) free(loop.break_jumps);

            c->current_loop = loop.outer;
            end_scope(c, line);
            break;
        }
        case NODE_SWITCH: {
            c->next_reg = c->local_count;
            int val_reg = allocate_reg(c);
            compile_expr(c, n->switchstmt.expr, val_reg);

            /* Register a pseudo-loop so `break` inside switch jumps to the end. */
            Loop sw;
            sw.start_ip = 0;
            sw.scope_depth = c->scope_depth;
            sw.break_jumps = NULL;
            sw.break_count = 0;
            sw.break_cap = 0;
            sw.is_switch = 1;
            sw.outer = c->current_loop;
            c->current_loop = &sw;

            int case_count = n->switchstmt.cases.count;
            int *case_jumps = case_count > 0 ? malloc(sizeof(int) * (size_t)case_count) : NULL;

            for (int i = 0; i < case_count; i++) {
                AstNode *cn = n->switchstmt.cases.items[i];
                c->next_reg = c->local_count + 1; /* keep val_reg alive */
                int cval = compile_expr_to_any_reg(c, cn->casestmt.value);
                c->next_reg = c->local_count + 1;
                int cmp = allocate_reg(c);
                emit_4(c, VM_OP_EQ, cmp, val_reg, cval, line);
                case_jumps[i] = emit_jump_cond(c, VM_OP_JUMP_IF_TRUE, cmp, line);
            }

            /* default branch (falls through when no case matched) */
            begin_scope(c, line);
            for (int j = 0; j < n->switchstmt.default_case.count; j++) {
                compile_stmt(c, n->switchstmt.default_case.items[j]);
            }
            end_scope(c, line);

            int end_jump = emit_jump(c, VM_OP_JUMP, line);

            for (int i = 0; i < case_count; i++) {
                patch_jump(c, case_jumps[i]);
                AstNode *cn = n->switchstmt.cases.items[i];
                begin_scope(c, line);
                for (int j = 0; j < cn->casestmt.body.count; j++) {
                    compile_stmt(c, cn->casestmt.body.items[j]);
                }
                end_scope(c, line);
                case_jumps[i] = emit_jump(c, VM_OP_JUMP, line);
            }
            if (case_jumps) {
                for (int i = 0; i < case_count; i++) {
                    patch_jump(c, case_jumps[i]);
                }
                free(case_jumps);
            }
            patch_jump(c, end_jump);

            for (int j = 0; j < sw.break_count; j++) {
                patch_jump(c, sw.break_jumps[j]);
            }
            if (sw.break_jumps) free(sw.break_jumps);

            c->current_loop = sw.outer;
            c->next_reg = c->local_count;
            break;
        }
        case NODE_IMPORT: {
            Value path_val = value_string(n->import_stmt.path);
            int path_idx = luna_chunk_add_constant(c->chunk, path_val);
            emit_opcode(c, VM_OP_IMPORT, line);
            emit_16(c, path_idx, line);
            emit_byte(c, (uint8_t)n->import_stmt.name_count, line);
            for (int i = 0; i < n->import_stmt.name_count; i++) {
                Value name_val = value_string(n->import_stmt.names[i]);
                int name_idx = luna_chunk_add_constant(c->chunk, name_val);
                emit_16(c, name_idx, line);
            }
            emit_byte(c, n->import_stmt.is_module_use ? 1 : 0, line);
            break;
        }
        case NODE_UNSAFE: {
            emit_opcode(c, VM_OP_UNSAFE_BEGIN, line);
            begin_scope(c, line);
            for (int i = 0; i < n->unsafe_block.body.count; i++) {
                compile_stmt(c, n->unsafe_block.body.items[i]);
            }
            end_scope(c, line);
            emit_opcode(c, VM_OP_UNSAFE_END, line);
            break;
        }
        case NODE_BREAK: {
            if (!c->current_loop) {
                fprintf(stderr, "Compile error: break statement outside loop\n");
                abort();
            }
            // Emit jump to patch later
            int jump = emit_jump(c, VM_OP_JUMP, line);
            Loop *loop = c->current_loop;
            if (loop->break_count >= loop->break_cap) {
                loop->break_cap = loop->break_cap ? loop->break_cap * 2 : 4;
                loop->break_jumps = realloc(loop->break_jumps, loop->break_cap * sizeof(int));
            }
            loop->break_jumps[loop->break_count++] = jump;
            break;
        }
        case NODE_CONTINUE: {
            Loop *target = c->current_loop;
            while (target && target->is_switch) target = target->outer;
            if (!target) {
                fprintf(stderr, "Compile error: continue statement outside loop\n");
                abort();
            }
            emit_loop_jump(c, target->start_ip, line);
            break;
        }
        case NODE_RETURN: {
            int val_reg = compile_expr_to_any_reg(c, n->ret.expr);
            emit_2(c, VM_OP_RETURN, val_reg, line);
            c->next_reg = old_reg;
            break;
        }
        case NODE_FUNC_DEF: {
            int dst = compile_function_value(c, n, line);

            if (n->funcdef.name) {
                if (c->parent == NULL && c->scope_depth == 0) {
                    /* Top-level function: define a global. */
                    Value name_val = value_string(n->funcdef.name);
                    int name_idx = luna_chunk_add_constant(c->chunk, name_val);
                    emit_opcode(c, VM_OP_SET_GLOBAL, line);
                    emit_16(c, name_idx, line);
                    emit_byte(c, (uint8_t)dst, line);
                } else {
                    // Define function name in current scope
                    add_local(c, n->funcdef.name, line);
                    int local_reg = c->locals[c->local_count - 1].reg;
                    emit_3(c, VM_OP_MOVE, local_reg, dst, line);
                }
            }
            c->next_reg = old_reg;
            break;
        }
        case NODE_GROUP: {
            for (int i = 0; i < n->block.items.count; i++) {
                compile_stmt(c, n->block.items.items[i]);
            }
            break;
        }
        case NODE_DATA_DEF:
        case NODE_BLOC_DEF:
            break;
        default:
            // Evaluate as stmt expression
            compile_expr_to_any_reg(c, n);
            c->next_reg = old_reg;
            break;
    }
    c->next_reg = c->local_count;
}

LunaChunk *luna_compile_program(AstNode *program_ast) {
    Compiler c;
    compiler_init(&c, NULL, "<main>");
    
    if (program_ast->kind == NODE_BLOCK) {
        for (int i = 0; i < program_ast->block.items.count; i++) {
            compile_stmt(&c, program_ast->block.items.items[i]);
        }
    } else {
        compile_stmt(&c, program_ast);
    }

    // Emit exit halt
    emit_opcode(&c, VM_OP_HALT, program_ast->line);

    c.chunk->reg_count = c.max_regs;
    #ifdef LUNA_VM_DEBUG
    printf("[COMPILER] Compiled chunk %s: %zu bytes of bytecode, %zu constants, %d registers\n",
           c.chunk->name, c.chunk->code_len, c.chunk->const_len, c.chunk->reg_count);
    #endif
    return c.chunk;
}
