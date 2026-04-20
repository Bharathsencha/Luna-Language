// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use std::sync::Arc;

use luna_data_rt::bloc::{BlocFieldKind, BlocFieldSpec, BlocLayout};
use luna_data_rt::luna_box::{BoxError, LunaBox};
use luna_data_rt::schema::{TemplateFieldKind, TemplateFieldSpec, TemplateSchema};
use luna_data_rt::template::{TemplateError, TemplateObject, TemplateValue};

#[test]
fn nested_bloc_layouts_stay_within_cap() {
    let vec2 = BlocLayout::new(
        "Vec2",
        vec![
            BlocFieldSpec::new("x", BlocFieldKind::Float),
            BlocFieldSpec::new("y", BlocFieldKind::Float),
        ],
    )
    .expect("vec2 should fit");

    let rect = BlocLayout::new(
        "Rect",
        vec![
            BlocFieldSpec::new(
                "min",
                BlocFieldKind::Nested {
                    name: "Vec2".to_string(),
                    size: vec2.size,
                },
            ),
            BlocFieldSpec::new(
                "max",
                BlocFieldKind::Nested {
                    name: "Vec2".to_string(),
                    size: vec2.size,
                },
            ),
        ],
    )
    .expect("rect should still fit inside the phase-1 cap");

    assert_eq!(rect.size, 32);
    assert_eq!(rect.field("max").expect("max field").offset, 16);
}

#[test]
fn box_write_rejects_oob_spans() {
    let mut buf = LunaBox::new(8).expect("box allocation should succeed");
    assert_eq!(
        buf.write(6, &[1, 2, 3]).expect_err("write must overflow"),
        BoxError::OutOfBounds {
            offset: 6,
            len: 3,
            capacity: 8,
        }
    );
}

#[test]
fn template_unknown_field_reports_error() {
    let schema = Arc::new(
        TemplateSchema::new(
            "Player",
            vec![
                TemplateFieldSpec::new("name", TemplateFieldKind::String),
                TemplateFieldSpec::new("hp", TemplateFieldKind::Primitive),
            ],
        )
        .expect("schema should build"),
    );

    let mut obj = TemplateObject::new(
        schema,
        vec![
            TemplateValue::String("Astra".to_string()),
            TemplateValue::Int(100),
        ],
    )
    .expect("object should build");

    assert_eq!(
        obj.get("missing").expect_err("field should not exist"),
        TemplateError::UnknownField("missing".to_string())
    );
    assert_eq!(
        obj.set("missing", TemplateValue::Null)
            .expect_err("field should not exist"),
        TemplateError::UnknownField("missing".to_string())
    );
}