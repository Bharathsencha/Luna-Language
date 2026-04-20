// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use crate::error;
use crate::table::{AllocMeta, AllocTable};

fn owned_meta(table: &AllocTable, ptr: usize) -> Result<(usize, AllocMeta), i32> {
    table
        .find_owner(ptr)
        .map(|(base, meta)| (base, *meta))
        .ok_or(error::ERR_UNKNOWN_POINTER)
}

pub fn check_free(table: &mut AllocTable, ptr: usize) -> i32 {
    if let Some(meta) = table.get_exact_mut(ptr) {
        if meta.poisoned {
            error::ERR_USE_AFTER_FREE
        } else {
            meta.poisoned = true;
            error::OK
        }
    } else {
        error::ERR_UNKNOWN_POINTER
    }
}

pub fn check_deref(table: &AllocTable, ptr: usize) -> i32 {
    match owned_meta(table, ptr) {
        Ok((_, meta)) if meta.poisoned => error::ERR_USE_AFTER_FREE,
        Ok(_) => error::OK,
        Err(code) => code,
    }
}

pub fn check_store(table: &AllocTable, ptr: usize) -> i32 {
    check_deref(table, ptr)
}

pub fn check_ptr_add(table: &AllocTable, base: usize, offset: usize) -> Result<usize, i32> {
    match table.get_exact(base).copied() {
        Some(meta) if meta.poisoned => Err(error::ERR_USE_AFTER_FREE),
        Some(meta) => {
            if offset >= meta.size {
                Err(error::ERR_OUT_OF_BOUNDS)
            } else {
                Ok(base.saturating_add(offset))
            }
        }
        None => Err(error::ERR_UNKNOWN_POINTER),
    }
}

pub fn check_addr(table: &AllocTable, ptr: usize, is_named: bool) -> i32 {
    if !is_named {
        return error::ERR_ADDR_TEMPORARY;
    }
    if table.contains_pointer_value(ptr) {
        return error::ERR_POINTER_TO_POINTER;
    }
    error::OK
}

pub fn check_escape(_ptr: usize) -> i32 {
    error::ERR_ESCAPE
}

pub fn check_cmp(table: &AllocTable, ptr_a: usize, ptr_b: usize, op: i32) -> i32 {
    if op == 0 || op == 1 {
        return error::OK;
    }
    let a = match owned_meta(table, ptr_a) {
        Ok(meta) => meta,
        Err(code) => return code,
    };
    let b = match owned_meta(table, ptr_b) {
        Ok(meta) => meta,
        Err(code) => return code,
    };
    if a.0 == b.0 {
        error::OK
    } else {
        error::ERR_CMP_BASE
    }
}

pub fn check_cast(target_type: i32) -> i32 {
    if target_type == 0 {
        error::OK
    } else {
        error::ERR_CAST
    }
}

pub fn check_call(is_builtin: bool) -> i32 {
    if is_builtin {
        error::OK
    } else {
        error::ERR_CALL
    }
}

pub fn check_alloc_size(n: i64, is_integer: bool) -> i32 {
    if !is_integer || n <= 0 {
        error::ERR_ALLOC_SIZE
    } else {
        error::OK
    }
}

pub fn check_gc_store(_ptr: usize) -> i32 {
    error::ERR_GC_STORE
}