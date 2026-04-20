// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

mod block;
mod defer;
mod error;
mod rules;
mod table;

use std::ffi::c_char;
use std::ptr;
use std::slice;
use std::sync::{Mutex, OnceLock};

use block::BlockState;
use defer::DeferStack;
use table::AllocTable;

struct Runtime {
    table: AllocTable,
    defer_stack: DeferStack,
    block: BlockState,
}

impl Runtime {
    fn new() -> Self {
        Self {
            table: AllocTable::default(),
            defer_stack: DeferStack::default(),
            block: BlockState::default(),
        }
    }

    fn reset(&mut self) {
        self.table.clear();
        self.defer_stack.clear();
        self.block.exit();
    }
}

fn runtime() -> &'static Mutex<Runtime> {
    static RUNTIME: OnceLock<Mutex<Runtime>> = OnceLock::new();
    RUNTIME.get_or_init(|| Mutex::new(Runtime::new()))
}

#[no_mangle]
pub extern "C" fn luna_mem_init() {
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    rt.reset();
}

#[no_mangle]
pub extern "C" fn luna_mem_begin() -> i32 {
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    rt.block.enter()
}

#[no_mangle]
pub extern "C" fn luna_mem_end() {
    luna_mem_run_defers();
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    rt.block.exit();
}

#[no_mangle]
pub extern "C" fn luna_mem_track_alloc(ptr: usize, size: usize) {
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    rt.table.register(ptr, size);
}

#[no_mangle]
pub extern "C" fn luna_mem_free_ok(ptr: usize) -> i32 {
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    rules::check_free(&mut rt.table, ptr)
}

#[no_mangle]
pub extern "C" fn luna_mem_deref_ok(ptr: usize) -> i32 {
    let rt = runtime().lock().expect("runtime mutex poisoned");
    rules::check_deref(&rt.table, ptr)
}

#[no_mangle]
pub extern "C" fn luna_mem_store_ok(ptr: usize) -> i32 {
    let rt = runtime().lock().expect("runtime mutex poisoned");
    rules::check_store(&rt.table, ptr)
}

#[no_mangle]
pub extern "C" fn luna_mem_ptr_add_ok(base: usize, offset: usize, out_ptr: *mut usize) -> i32 {
    let rt = runtime().lock().expect("runtime mutex poisoned");
    match rules::check_ptr_add(&rt.table, base, offset) {
        Ok(result) => {
            if !out_ptr.is_null() {
                unsafe { ptr::write(out_ptr, result) };
            }
            error::OK
        }
        Err(code) => code,
    }
}

#[no_mangle]
pub extern "C" fn luna_mem_addr_ok(ptr: usize, is_named: i32) -> i32 {
    let rt = runtime().lock().expect("runtime mutex poisoned");
    rules::check_addr(&rt.table, ptr, is_named != 0)
}

#[no_mangle]
pub extern "C" fn luna_mem_escape_ok(ptr: usize) -> i32 {
    rules::check_escape(ptr)
}

#[no_mangle]
pub extern "C" fn luna_mem_cmp_ok(ptr_a: usize, ptr_b: usize, op: i32) -> i32 {
    let rt = runtime().lock().expect("runtime mutex poisoned");
    rules::check_cmp(&rt.table, ptr_a, ptr_b, op)
}

#[no_mangle]
pub extern "C" fn luna_mem_cast_ok(_ptr: usize, target_type: i32) -> i32 {
    rules::check_cast(target_type)
}

#[no_mangle]
pub extern "C" fn luna_mem_call_ok(is_builtin: i32) -> i32 {
    rules::check_call(is_builtin != 0)
}

#[no_mangle]
pub extern "C" fn luna_mem_alloc_size_ok(n: i64, is_integer: i32) -> i32 {
    rules::check_alloc_size(n, is_integer != 0)
}

#[no_mangle]
pub extern "C" fn luna_mem_defer(ptr: usize) {
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    rt.defer_stack.push(ptr);
}

#[no_mangle]
pub extern "C" fn luna_mem_run_defers() {
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    while let Some(ptr) = rt.defer_stack.pop() {
        let _ = rules::check_free(&mut rt.table, ptr);
    }
}

#[no_mangle]
pub extern "C" fn luna_mem_gc_store_ok(ptr: usize) -> i32 {
    rules::check_gc_store(ptr)
}

#[no_mangle]
pub extern "C" fn luna_mem_error_message(code: i32, buf: *mut c_char, buf_len: usize) {
    if buf.is_null() || buf_len == 0 {
        return;
    }

    let message = error::message(code).as_bytes();
    let copy_len = message.len().min(buf_len.saturating_sub(1));

    unsafe {
        let dest = slice::from_raw_parts_mut(buf as *mut u8, buf_len);
        if copy_len > 0 {
            dest[..copy_len].copy_from_slice(&message[..copy_len]);
        }
        dest[copy_len] = 0;
    }
}

#[no_mangle]
pub extern "C" fn luna_mem_shutdown() {
    let mut rt = runtime().lock().expect("runtime mutex poisoned");
    if rt.table.leak_count() > 0 {
        eprintln!(
            "luna unsafe runtime warning: {} allocation(s) were still live at shutdown",
            rt.table.leak_count()
        );
    }
    rt.reset();
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{Mutex, OnceLock};

    fn test_guard() -> std::sync::MutexGuard<'static, ()> {
        static TEST_LOCK: OnceLock<Mutex<()>> = OnceLock::new();
        TEST_LOCK
            .get_or_init(|| Mutex::new(()))
            .lock()
            .expect("test mutex poisoned")
    }

    #[test]
    fn enter_block_rejects_nesting() {
        let _guard = test_guard();
        luna_mem_init();
        assert_eq!(luna_mem_begin(), error::OK);
        assert_eq!(luna_mem_begin(), error::ERR_NESTED_UNSAFE);
        luna_mem_end();
    }

    #[test]
    fn alloc_tracking_and_poisoning_work() {
        let _guard = test_guard();
        luna_mem_init();
        luna_mem_track_alloc(100, 8);
        assert_eq!(luna_mem_deref_ok(100), error::OK);
        assert_eq!(luna_mem_ptr_add_ok(100, 4, ptr::null_mut()), error::OK);
        assert_eq!(luna_mem_ptr_add_ok(100, 8, ptr::null_mut()), error::ERR_OUT_OF_BOUNDS);
        assert_eq!(luna_mem_free_ok(100), error::OK);
        assert_eq!(luna_mem_deref_ok(100), error::ERR_USE_AFTER_FREE);
    }

    #[test]
    fn compare_and_cast_rules_work() {
        let _guard = test_guard();
        luna_mem_init();
        luna_mem_track_alloc(100, 8);
        luna_mem_track_alloc(200, 8);
        assert_eq!(luna_mem_cmp_ok(100, 102, 2), error::OK);
        assert_eq!(luna_mem_cmp_ok(100, 200, 2), error::ERR_CMP_BASE);
        assert_eq!(luna_mem_cast_ok(100, 0), error::OK);
        assert_eq!(luna_mem_cast_ok(100, 1), error::ERR_CAST);
    }

    #[test]
    fn defer_marks_registered_allocations_freed() {
        let _guard = test_guard();
        luna_mem_init();
        luna_mem_track_alloc(100, 8);
        luna_mem_defer(100);
        luna_mem_run_defers();
        assert_eq!(luna_mem_deref_ok(100), error::ERR_USE_AFTER_FREE);
    }

    #[test]
    fn addr_and_call_checks_work() {
        let _guard = test_guard();
        luna_mem_init();
        luna_mem_track_alloc(100, 8);
        assert_eq!(luna_mem_addr_ok(10, 1), error::OK);
        assert_eq!(luna_mem_addr_ok(100, 1), error::ERR_POINTER_TO_POINTER);
        assert_eq!(luna_mem_addr_ok(10, 0), error::ERR_ADDR_TEMPORARY);
        assert_eq!(luna_mem_call_ok(1), error::OK);
        assert_eq!(luna_mem_call_ok(0), error::ERR_CALL);
    }
}