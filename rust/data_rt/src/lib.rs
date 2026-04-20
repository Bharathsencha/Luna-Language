// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

pub mod bloc;
pub mod kinds;
pub mod luna_box;
pub mod schema;
pub mod template;

use std::collections::HashMap;
use std::ffi::{c_char, CStr};
use std::slice;
use std::sync::{Mutex, OnceLock};

use bloc::{BlocFieldKind, BlocFieldSpec, BlocLayout};
use kinds::BOX_MAX_BYTES;
use schema::{SchemaRegistry, TemplateFieldKind, TemplateFieldSpec, TemplateSchema};

// Error codes
const OK: i32 = 0;
const ERR_BLOC_EMPTY_NAME: i32 = 1;
const ERR_BLOC_DUPLICATE_FIELD: i32 = 2;
const ERR_BLOC_OVERSIZE: i32 = 3;
const ERR_BOX_INVALID_SIZE: i32 = 4;
const ERR_BOX_USE_AFTER_FREE: i32 = 5;
const ERR_BOX_NOT_LIVE: i32 = 6;
const ERR_TEMPLATE_EMPTY_NAME: i32 = 7;
const ERR_TEMPLATE_DUPLICATE_FIELD: i32 = 8;
const ERR_TEMPLATE_DUPLICATE_SCHEMA: i32 = 9;
const ERR_TEMPLATE_ARITY: i32 = 10;
const ERR_TEMPLATE_UNKNOWN_FIELD: i32 = 11;
const ERR_CONTAINMENT: i32 = 12;

fn error_message(code: i32) -> &'static str {
    match code {
        OK => "ok",
        ERR_BLOC_EMPTY_NAME => "bloc name cannot be empty",
        ERR_BLOC_DUPLICATE_FIELD => "bloc has duplicate field names",
        ERR_BLOC_OVERSIZE => "bloc exceeds the cache-line size limit (32 bytes)",
        ERR_BOX_INVALID_SIZE => "box size must be between 1 and 4096 bytes",
        ERR_BOX_USE_AFTER_FREE => "box was already freed (use-after-free)",
        ERR_BOX_NOT_LIVE => "box is not live",
        ERR_TEMPLATE_EMPTY_NAME => "template name cannot be empty",
        ERR_TEMPLATE_DUPLICATE_FIELD => "template has duplicate field names",
        ERR_TEMPLATE_DUPLICATE_SCHEMA => "template with this name is already registered",
        ERR_TEMPLATE_ARITY => "template constructor argument count does not match field count",
        ERR_TEMPLATE_UNKNOWN_FIELD => "template does not have a field with that name",
        ERR_CONTAINMENT => {
            "containment hierarchy violation: type cannot be stored inside this tier"
        }
        _ => "unknown data runtime error",
    }
}

// Global state
struct DataRuntime {
    schema_registry: SchemaRegistry,
    box_live: HashMap<u64, bool>, // handle → is_live
}

impl DataRuntime {
    fn new() -> Self {
        Self {
            schema_registry: SchemaRegistry::default(),
            box_live: HashMap::new(),
        }
    }

    fn reset(&mut self) {
        self.schema_registry = SchemaRegistry::default();
        self.box_live.clear();
    }
}

fn runtime() -> &'static Mutex<DataRuntime> {
    static RT: OnceLock<Mutex<DataRuntime>> = OnceLock::new();
    RT.get_or_init(|| Mutex::new(DataRuntime::new()))
}

// Helpers
unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> Option<&'a str> {
    if ptr.is_null() {
        return None;
    }
    CStr::from_ptr(ptr).to_str().ok()
}

unsafe fn cstr_array_to_vec(fields: *const *const c_char, count: i32) -> Vec<String> {
    if fields.is_null() || count <= 0 {
        return Vec::new();
    }
    let ptrs = slice::from_raw_parts(fields, count as usize);
    ptrs.iter()
        .filter_map(|p| cstr_to_str(*p).map(String::from))
        .collect()
}

// Containment kinds
// Match the C-side enum values: BLOC=1, BOX=2, TEMPLATE=3
const KIND_BLOC: i32 = 1;
const KIND_BOX: i32 = 2;
const KIND_TEMPLATE: i32 = 3;

// FFI exports

#[no_mangle]
pub extern "C" fn luna_data_init() {
    let mut rt = runtime().lock().expect("data runtime mutex poisoned");
    rt.reset();
}

/// Validate a bloc layout: name non-empty, no duplicate fields, fits in cache line.
#[no_mangle]
pub unsafe extern "C" fn luna_data_bloc_validate(
    name: *const c_char,
    fields: *const *const c_char,
    count: i32,
) -> i32 {
    let name_str = match cstr_to_str(name) {
        Some(s) if !s.trim().is_empty() => s,
        _ => return ERR_BLOC_EMPTY_NAME,
    };

    let field_names = cstr_array_to_vec(fields, count);

    // Build specs — treat all fields as Int (8 bytes) for worst-case sizing
    let specs: Vec<BlocFieldSpec> = field_names
        .iter()
        .map(|n| BlocFieldSpec::new(n.clone(), BlocFieldKind::Int))
        .collect();

    match BlocLayout::new(name_str, specs) {
        Ok(_) => OK,
        Err(bloc::BlocLayoutError::EmptyName) => ERR_BLOC_EMPTY_NAME,
        Err(bloc::BlocLayoutError::DuplicateField(_)) => ERR_BLOC_DUPLICATE_FIELD,
        Err(bloc::BlocLayoutError::Oversize { .. }) => ERR_BLOC_OVERSIZE,
    }
}

/// Validate box allocation size.
#[no_mangle]
pub extern "C" fn luna_data_box_size_ok(size: usize) -> i32 {
    if size == 0 || size > BOX_MAX_BYTES {
        ERR_BOX_INVALID_SIZE
    } else {
        OK
    }
}

/// Register a box handle as live.
#[no_mangle]
pub extern "C" fn luna_data_box_track(handle: u64) {
    let mut rt = runtime().lock().expect("data runtime mutex poisoned");
    rt.box_live.insert(handle, true);
}

/// Check if a box handle is live (not freed).
#[no_mangle]
pub extern "C" fn luna_data_box_access_ok(handle: u64) -> i32 {
    let rt = runtime().lock().expect("data runtime mutex poisoned");
    match rt.box_live.get(&handle) {
        Some(true) => OK,
        Some(false) => ERR_BOX_USE_AFTER_FREE,
        None => ERR_BOX_NOT_LIVE,
    }
}

/// Mark a box handle as freed. Returns error if already freed.
#[no_mangle]
pub extern "C" fn luna_data_box_free_ok(handle: u64) -> i32 {
    let mut rt = runtime().lock().expect("data runtime mutex poisoned");
    match rt.box_live.get_mut(&handle) {
        Some(live) if *live => {
            *live = false;
            OK
        }
        Some(_) => ERR_BOX_USE_AFTER_FREE,
        None => ERR_BOX_NOT_LIVE,
    }
}

/// Validate and register a template schema.
#[no_mangle]
pub unsafe extern "C" fn luna_data_template_register(
    name: *const c_char,
    fields: *const *const c_char,
    count: i32,
) -> i32 {
    let name_str = match cstr_to_str(name) {
        Some(s) if !s.trim().is_empty() => s,
        _ => return ERR_TEMPLATE_EMPTY_NAME,
    };

    let field_names = cstr_array_to_vec(fields, count);
    let specs: Vec<TemplateFieldSpec> = field_names
        .iter()
        .map(|n| TemplateFieldSpec::new(n.clone(), TemplateFieldKind::Dynamic))
        .collect();

    let schema = match TemplateSchema::new(name_str, specs) {
        Ok(s) => s,
        Err(schema::SchemaError::EmptyName) => return ERR_TEMPLATE_EMPTY_NAME,
        Err(schema::SchemaError::DuplicateField(_)) => return ERR_TEMPLATE_DUPLICATE_FIELD,
        Err(schema::SchemaError::DuplicateSchema(_)) => return ERR_TEMPLATE_DUPLICATE_SCHEMA,
    };

    let mut rt = runtime().lock().expect("data runtime mutex poisoned");
    match rt.schema_registry.register(std::sync::Arc::new(schema)) {
        Ok(()) => OK,
        Err(schema::SchemaError::DuplicateSchema(_)) => {
            // Already registered with same name — that's OK for re-declaration
            OK
        }
        Err(_) => ERR_TEMPLATE_DUPLICATE_SCHEMA,
    }
}

/// Check constructor arity against registered schema.
#[no_mangle]
pub unsafe extern "C" fn luna_data_template_arity_ok(name: *const c_char, got: i32) -> i32 {
    let name_str = match cstr_to_str(name) {
        Some(s) => s,
        None => return ERR_TEMPLATE_EMPTY_NAME,
    };

    let rt = runtime().lock().expect("data runtime mutex poisoned");
    match rt.schema_registry.get(name_str) {
        Some(schema) => {
            if schema.fields.len() == got as usize {
                OK
            } else {
                ERR_TEMPLATE_ARITY
            }
        }
        None => OK, // Schema not registered — let C handle it
    }
}

/// Check a field name exists in a registered template schema.
#[no_mangle]
pub unsafe extern "C" fn luna_data_template_field_ok(
    name: *const c_char,
    field: *const c_char,
) -> i32 {
    let name_str = match cstr_to_str(name) {
        Some(s) => s,
        None => return ERR_TEMPLATE_EMPTY_NAME,
    };
    let field_str = match cstr_to_str(field) {
        Some(s) => s,
        None => return ERR_TEMPLATE_UNKNOWN_FIELD,
    };

    let rt = runtime().lock().expect("data runtime mutex poisoned");
    match rt.schema_registry.get(name_str) {
        Some(schema) => {
            if schema.field(field_str).is_some() {
                OK
            } else {
                ERR_TEMPLATE_UNKNOWN_FIELD
            }
        }
        None => OK, // Schema not registered — let C handle it
    }
}

/// Enforce the containment hierarchy:
///   Template can contain Box and Bloc
///   Box can contain Bloc only
///   Bloc is a leaf node
#[no_mangle]
pub extern "C" fn luna_data_containment_ok(outer_kind: i32, inner_kind: i32) -> i32 {
    match (outer_kind, inner_kind) {
        // Template can hold Bloc and Box
        (KIND_TEMPLATE, KIND_BLOC) => OK,
        (KIND_TEMPLATE, KIND_BOX) => OK,
        // Box can hold Bloc only
        (KIND_BOX, KIND_BLOC) => OK,
        // Everything else is a violation
        _ => ERR_CONTAINMENT,
    }
}

/// Get human-readable error message for an error code.
#[no_mangle]
pub extern "C" fn luna_data_error_message(code: i32, buf: *mut c_char, buf_len: usize) {
    if buf.is_null() || buf_len == 0 {
        return;
    }

    let message = error_message(code).as_bytes();
    let copy_len = message.len().min(buf_len.saturating_sub(1));

    unsafe {
        let dest = slice::from_raw_parts_mut(buf as *mut u8, buf_len);
        if copy_len > 0 {
            dest[..copy_len].copy_from_slice(&message[..copy_len]);
        }
        dest[copy_len] = 0;
    }
}

/// Cleanup.
#[no_mangle]
pub extern "C" fn luna_data_shutdown() {
    let mut rt = runtime().lock().expect("data runtime mutex poisoned");
    let leaked_boxes = rt.box_live.values().filter(|&&live| live).count();
    if leaked_boxes > 0 {
        eprintln!(
            "luna data runtime warning: {} box(es) were still live at shutdown",
            leaked_boxes
        );
    }
    rt.reset();
}

#[cfg(test)]
mod tests {
    use std::sync::Arc;

    use crate::bloc::{BlocFieldKind, BlocFieldSpec, BlocLayout, BlocLayoutError};
    use crate::kinds::{RuntimeKind, BLOC_MAX_INLINE_BYTES, BOX_MAX_BYTES};
    use crate::luna_box::{BoxError, LunaBox};
    use crate::schema::{
        SchemaError, SchemaRegistry, TemplateFieldKind, TemplateFieldSpec, TemplateSchema,
    };
    use crate::template::{TemplateError, TemplateObject, TemplateValue};

    use super::*;
    use std::sync::{Mutex as StdMutex, OnceLock as StdOnceLock};

    fn test_guard() -> std::sync::MutexGuard<'static, ()> {
        static TEST_LOCK: StdOnceLock<StdMutex<()>> = StdOnceLock::new();
        TEST_LOCK
            .get_or_init(|| StdMutex::new(()))
            .lock()
            .expect("test mutex poisoned")
    }

    #[test]
    fn bloc_layout_tracks_offsets() {
        let layout = BlocLayout::new(
            "Vec2",
            vec![
                BlocFieldSpec::new("x", BlocFieldKind::Float),
                BlocFieldSpec::new("y", BlocFieldKind::Float),
            ],
        )
        .expect("vec2 layout should fit");

        assert_eq!(layout.header.kind, RuntimeKind::Bloc as u8);
        assert_eq!(layout.size, 16);
        assert_eq!(layout.field("x").expect("x field").offset, 0);
        assert_eq!(layout.field("y").expect("y field").offset, 8);
    }

    #[test]
    fn bloc_layout_rejects_oversize_values() {
        let err = BlocLayout::new(
            "TooWide",
            vec![
                BlocFieldSpec::new("a", BlocFieldKind::Int),
                BlocFieldSpec::new("b", BlocFieldKind::Int),
                BlocFieldSpec::new("c", BlocFieldKind::Int),
                BlocFieldSpec::new("d", BlocFieldKind::Int),
                BlocFieldSpec::new("e", BlocFieldKind::Bool),
            ],
        )
        .expect_err("layout should overflow the phase-1 cap");

        assert_eq!(
            err,
            BlocLayoutError::Oversize {
                size: BLOC_MAX_INLINE_BYTES + 1,
                max: BLOC_MAX_INLINE_BYTES,
            }
        );
    }

    #[test]
    fn box_buffer_supports_fixed_size_reads_and_writes() {
        let mut buf = LunaBox::new(8).expect("box allocation should succeed");
        assert_eq!(buf.header.kind, RuntimeKind::Box as u8);
        buf.write(2, &[1, 2, 3]).expect("write should fit");
        assert_eq!(buf.read(2, 3).expect("read should fit"), &[1, 2, 3]);
        assert!(buf.cap() >= buf.len());
    }

    #[test]
    fn box_buffer_enforces_limits_and_free_state() {
        assert_eq!(
            LunaBox::new(BOX_MAX_BYTES + 1).expect_err("oversize box must fail"),
            BoxError::InvalidSize {
                requested: BOX_MAX_BYTES + 1,
                max: BOX_MAX_BYTES,
            }
        );

        let mut buf = LunaBox::new(4).expect("box allocation should succeed");
        buf.free().expect("free should succeed");
        assert!(buf.is_freed());
        assert_eq!(
            buf.read(0, 1).expect_err("read after free must fail"),
            BoxError::UseAfterFree
        );
    }

    #[test]
    fn schema_registry_and_template_access_work() {
        let schema = Arc::new(
            TemplateSchema::new(
                "Player",
                vec![
                    TemplateFieldSpec::new("name", TemplateFieldKind::String),
                    TemplateFieldSpec::new("hp", TemplateFieldKind::Primitive),
                    TemplateFieldSpec::new("pos", TemplateFieldKind::Bloc),
                ],
            )
            .expect("schema should build"),
        );

        let mut registry = SchemaRegistry::default();
        registry
            .register(schema.clone())
            .expect("first register works");
        assert_eq!(
            registry
                .register(schema.clone())
                .expect_err("duplicate schema should fail"),
            SchemaError::DuplicateSchema("Player".to_string())
        );

        let mut player = TemplateObject::new(
            schema.clone(),
            vec![
                TemplateValue::String("Astra".to_string()),
                TemplateValue::Int(100),
                TemplateValue::BlocBytes(vec![0; 16]),
            ],
        )
        .expect("template construction should match schema");

        assert_eq!(
            player.get("name").expect("name field"),
            &TemplateValue::String("Astra".to_string())
        );
        player
            .set("hp", TemplateValue::Int(80))
            .expect("field update should work");
        assert_eq!(player.get("hp").expect("hp field"), &TemplateValue::Int(80));
    }

    #[test]
    fn template_constructor_validates_arity() {
        let schema = Arc::new(
            TemplateSchema::new(
                "Pair",
                vec![
                    TemplateFieldSpec::new("left", TemplateFieldKind::Dynamic),
                    TemplateFieldSpec::new("right", TemplateFieldKind::Dynamic),
                ],
            )
            .expect("schema should build"),
        );

        assert_eq!(
            TemplateObject::new(schema, vec![TemplateValue::Null])
                .expect_err("arity mismatch should fail"),
            TemplateError::ArityMismatch {
                expected: 2,
                got: 1,
            }
        );
    }

    #[test]
    fn ffi_box_lifecycle_works() {
        let _guard = test_guard();
        luna_data_init();

        assert_eq!(luna_data_box_size_ok(32), OK);
        assert_eq!(luna_data_box_size_ok(0), ERR_BOX_INVALID_SIZE);
        assert_eq!(
            luna_data_box_size_ok(BOX_MAX_BYTES + 1),
            ERR_BOX_INVALID_SIZE
        );

        luna_data_box_track(42);
        assert_eq!(luna_data_box_access_ok(42), OK);
        assert_eq!(luna_data_box_free_ok(42), OK);
        assert_eq!(luna_data_box_access_ok(42), ERR_BOX_USE_AFTER_FREE);
        assert_eq!(luna_data_box_free_ok(42), ERR_BOX_USE_AFTER_FREE);
    }

    #[test]
    fn ffi_containment_hierarchy_enforced() {
        // Valid containments
        assert_eq!(luna_data_containment_ok(KIND_TEMPLATE, KIND_BLOC), OK);
        assert_eq!(luna_data_containment_ok(KIND_TEMPLATE, KIND_BOX), OK);
        assert_eq!(luna_data_containment_ok(KIND_BOX, KIND_BLOC), OK);

        // Invalid containments
        assert_ne!(luna_data_containment_ok(KIND_BLOC, KIND_BOX), OK);
        assert_ne!(luna_data_containment_ok(KIND_BLOC, KIND_TEMPLATE), OK);
        assert_ne!(luna_data_containment_ok(KIND_BOX, KIND_BOX), OK);
        assert_ne!(luna_data_containment_ok(KIND_BOX, KIND_TEMPLATE), OK);
        assert_ne!(luna_data_containment_ok(KIND_TEMPLATE, KIND_TEMPLATE), OK);
    }
}
