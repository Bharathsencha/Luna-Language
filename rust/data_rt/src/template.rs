// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use std::collections::BTreeMap;
use std::sync::Arc;

use crate::kinds::{RuntimeHeader, RuntimeKind};
use crate::schema::TemplateSchema;

#[derive(Clone, Debug, PartialEq)]
pub enum TemplateValue {
    Null,
    Int(i64),
    Float(f64),
    Bool(bool),
    Char(char),
    String(String),
    List(Vec<TemplateValue>),
    Map(BTreeMap<String, TemplateValue>),
    ClosureHandle(usize),
    BoxHandle(usize),
    BlocBytes(Vec<u8>),
    Template(Box<TemplateObject>),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum TemplateError {
    ArityMismatch { expected: usize, got: usize },
    UnknownField(String),
}

#[derive(Clone, Debug, PartialEq)]
pub struct TemplateObject {
    pub header: RuntimeHeader,
    pub schema: Arc<TemplateSchema>,
    fields: Vec<TemplateValue>,
}

impl TemplateObject {
    pub fn new(
        schema: Arc<TemplateSchema>,
        fields: Vec<TemplateValue>,
    ) -> Result<Self, TemplateError> {
        if fields.len() != schema.fields.len() {
            return Err(TemplateError::ArityMismatch {
                expected: schema.fields.len(),
                got: fields.len(),
            });
        }

        Ok(Self {
            header: RuntimeHeader::new(RuntimeKind::Template),
            schema,
            fields,
        })
    }

    pub fn get(&self, name: &str) -> Result<&TemplateValue, TemplateError> {
        let slot = self
            .schema
            .slot_of(name)
            .ok_or_else(|| TemplateError::UnknownField(name.to_string()))?;
        Ok(&self.fields[slot])
    }

    pub fn set(&mut self, name: &str, value: TemplateValue) -> Result<(), TemplateError> {
        let slot = self
            .schema
            .slot_of(name)
            .ok_or_else(|| TemplateError::UnknownField(name.to_string()))?;
        self.fields[slot] = value;
        Ok(())
    }

    pub fn values(&self) -> &[TemplateValue] {
        &self.fields
    }
}