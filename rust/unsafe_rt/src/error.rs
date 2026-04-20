// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

pub const OK: i32 = 0;
pub const ERR_POINTER_TO_POINTER: i32 = 1;
pub const ERR_ESCAPE: i32 = 2;
pub const ERR_OUT_OF_BOUNDS: i32 = 3;
pub const ERR_USE_AFTER_FREE: i32 = 4;
pub const ERR_ADDR_TEMPORARY: i32 = 5;
pub const ERR_NESTED_UNSAFE: i32 = 6;
pub const ERR_GC_STORE: i32 = 7;
pub const ERR_ALLOC_SIZE: i32 = 8;
pub const ERR_CMP_BASE: i32 = 9;
pub const ERR_CAST: i32 = 10;
pub const ERR_CALL: i32 = 11;
pub const ERR_CONDITIONAL_FREE: i32 = 12;
pub const ERR_UNKNOWN_POINTER: i32 = 13;

pub fn message(code: i32) -> &'static str {
    match code {
        OK => "ok",
        ERR_POINTER_TO_POINTER => "cannot take address_of() of a pointer value",
        ERR_ESCAPE => "pointer cannot escape the current unsafe block",
        ERR_OUT_OF_BOUNDS => "ptr_add() moved outside the allocation bounds",
        ERR_USE_AFTER_FREE => "pointer was freed or freed twice",
        ERR_ADDR_TEMPORARY => "address_of() only works on named values",
        ERR_NESTED_UNSAFE => "nested unsafe blocks are not allowed",
        ERR_GC_STORE => "pointers cannot be stored inside GC-managed values",
        ERR_ALLOC_SIZE => "alloc() size must be a positive integer",
        ERR_CMP_BASE => "ordered pointer comparisons need the same base allocation",
        ERR_CAST => "pointers may only be cast to int",
        ERR_CALL => "only builtin calls are allowed inside unsafe blocks",
        ERR_CONDITIONAL_FREE => "free() may not appear inside conditional control flow",
        ERR_UNKNOWN_POINTER => "unsafe error: pointer is not registered in the runtime",
        _ => "unsafe error: unknown runtime error",
    }
}