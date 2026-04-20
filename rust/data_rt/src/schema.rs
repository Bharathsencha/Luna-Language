// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use std::collections::{HashMap, HashSet};
use std::sync::Arc;

use crate::kinds::{RuntimeHeader, RuntimeKind};

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum TemplateFieldKind {
    Primitive,
    String,
    List,
    Map,
    Closure,
    Template,
    Bloc,
    BoxHandle,
    Null,
    Dynamic,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TemplateFieldSpec {
    pub name: String,
    pub kind: TemplateFieldKind,
}

impl TemplateFieldSpec {
    pub fn new(name: impl Into<String>, kind: TemplateFieldKind) -> Self {
        Self {
            name: name.into(),
            kind,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TemplateField {
    pub name: String,
    pub kind: TemplateFieldKind,
    pub slot: usize,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum SchemaError {
    EmptyName,
    DuplicateField(String),
    DuplicateSchema(String),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TemplateSchema {
    pub header: RuntimeHeader,
    pub name: String,
    pub fields: Vec<TemplateField>,
    field_index: HashMap<String, usize>,
}

impl TemplateSchema {
    pub fn new(
        name: impl Into<String>,
        specs: Vec<TemplateFieldSpec>,
    ) -> Result<Self, SchemaError> {
        let name = name.into();
        if name.trim().is_empty() {
            return Err(SchemaError::EmptyName);
        }

        let mut seen = HashSet::new();
        let mut fields = Vec::with_capacity(specs.len());
        let mut field_index = HashMap::with_capacity(specs.len());

        for (slot, spec) in specs.into_iter().enumerate() {
            if !seen.insert(spec.name.clone()) {
                return Err(SchemaError::DuplicateField(spec.name));
            }
            field_index.insert(spec.name.clone(), slot);
            fields.push(TemplateField {
                name: spec.name,
                kind: spec.kind,
                slot,
            });
        }

        Ok(Self {
            header: RuntimeHeader::new(RuntimeKind::Template),
            name,
            fields,
            field_index,
        })
    }

    pub fn field(&self, name: &str) -> Option<&TemplateField> {
        self.field_index
            .get(name)
            .and_then(|slot| self.fields.get(*slot))
    }

    pub fn slot_of(&self, name: &str) -> Option<usize> {
        self.field_index.get(name).copied()
    }
}

#[derive(Default)]
pub struct SchemaRegistry {
    schemas: HashMap<String, Arc<TemplateSchema>>,
}

impl SchemaRegistry {
    pub fn register(&mut self, schema: Arc<TemplateSchema>) -> Result<(), SchemaError> {
        if self.schemas.contains_key(&schema.name) {
            return Err(SchemaError::DuplicateSchema(schema.name.clone()));
        }
        self.schemas.insert(schema.name.clone(), schema);
        Ok(())
    }

    pub fn get(&self, name: &str) -> Option<Arc<TemplateSchema>> {
        self.schemas.get(name).cloned()
    }
}