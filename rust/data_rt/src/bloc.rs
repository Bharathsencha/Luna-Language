// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use std::collections::HashSet;

use crate::kinds::{RuntimeHeader, RuntimeKind, BLOC_MAX_INLINE_BYTES};

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BlocLayoutError {
    EmptyName,
    DuplicateField(String),
    Oversize { size: usize, max: usize },
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BlocFieldKind {
    Int,
    Float,
    Bool,
    Char,
    Nested { name: String, size: usize },
}

impl BlocFieldKind {
    pub fn size_bytes(&self) -> usize {
        match self {
            Self::Int | Self::Float => 8,
            Self::Bool => 1,
            Self::Char => 4,
            Self::Nested { size, .. } => *size,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BlocFieldSpec {
    pub name: String,
    pub kind: BlocFieldKind,
}

impl BlocFieldSpec {
    pub fn new(name: impl Into<String>, kind: BlocFieldKind) -> Self {
        Self {
            name: name.into(),
            kind,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BlocField {
    pub name: String,
    pub kind: BlocFieldKind,
    pub offset: usize,
    pub size: usize,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BlocLayout {
    pub header: RuntimeHeader,
    pub name: String,
    pub fields: Vec<BlocField>,
    pub size: usize,
}

impl BlocLayout {
    pub fn new(
        name: impl Into<String>,
        specs: Vec<BlocFieldSpec>,
    ) -> Result<Self, BlocLayoutError> {
        let name = name.into();
        if name.trim().is_empty() {
            return Err(BlocLayoutError::EmptyName);
        }

        let mut seen = HashSet::new();
        let mut size = 0usize;
        let mut fields = Vec::with_capacity(specs.len());

        for spec in specs {
            if !seen.insert(spec.name.clone()) {
                return Err(BlocLayoutError::DuplicateField(spec.name));
            }

            let field_size = spec.kind.size_bytes();
            let next_size = size.saturating_add(field_size);
            if next_size > BLOC_MAX_INLINE_BYTES {
                return Err(BlocLayoutError::Oversize {
                    size: next_size,
                    max: BLOC_MAX_INLINE_BYTES,
                });
            }

            fields.push(BlocField {
                name: spec.name,
                kind: spec.kind,
                offset: size,
                size: field_size,
            });
            size = next_size;
        }

        Ok(Self {
            header: RuntimeHeader::new(RuntimeKind::Bloc),
            name,
            fields,
            size,
        })
    }

    pub fn field(&self, name: &str) -> Option<&BlocField> {
        self.fields.iter().find(|field| field.name == name)
    }
}