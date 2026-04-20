// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

pub const BLOC_MAX_INLINE_BYTES: usize = 32;
pub const BOX_MAX_BYTES: usize = 4096;

#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RuntimeKind {
    Bloc = 1,
    Box = 2,
    Template = 3,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RuntimeHeader {
    pub kind: u8,
}

impl RuntimeHeader {
    pub const fn new(kind: RuntimeKind) -> Self {
        Self { kind: kind as u8 }
    }
}