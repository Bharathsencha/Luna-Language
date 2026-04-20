// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use crate::kinds::{RuntimeHeader, RuntimeKind, BOX_MAX_BYTES};

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BoxError {
    InvalidSize {
        requested: usize,
        max: usize,
    },
    OutOfBounds {
        offset: usize,
        len: usize,
        capacity: usize,
    },
    UseAfterFree,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct LunaBox {
    pub header: RuntimeHeader,
    bytes: Vec<u8>,
    freed: bool,
}

impl LunaBox {
    pub fn new(size: usize) -> Result<Self, BoxError> {
        if size == 0 || size > BOX_MAX_BYTES {
            return Err(BoxError::InvalidSize {
                requested: size,
                max: BOX_MAX_BYTES,
            });
        }

        Ok(Self {
            header: RuntimeHeader::new(RuntimeKind::Box),
            bytes: vec![0; size],
            freed: false,
        })
    }

    pub fn len(&self) -> usize {
        self.bytes.len()
    }

    pub fn cap(&self) -> usize {
        self.bytes.capacity()
    }

    pub fn is_freed(&self) -> bool {
        self.freed
    }

    pub fn write(&mut self, offset: usize, data: &[u8]) -> Result<(), BoxError> {
        self.ensure_live()?;
        let end = offset.saturating_add(data.len());
        if end > self.bytes.len() {
            return Err(BoxError::OutOfBounds {
                offset,
                len: data.len(),
                capacity: self.bytes.len(),
            });
        }
        self.bytes[offset..end].copy_from_slice(data);
        Ok(())
    }

    pub fn read(&self, offset: usize, len: usize) -> Result<&[u8], BoxError> {
        self.ensure_live()?;
        let end = offset.saturating_add(len);
        if end > self.bytes.len() {
            return Err(BoxError::OutOfBounds {
                offset,
                len,
                capacity: self.bytes.len(),
            });
        }
        Ok(&self.bytes[offset..end])
    }

    pub fn free(&mut self) -> Result<(), BoxError> {
        self.ensure_live()?;
        self.bytes.clear();
        self.bytes.shrink_to_fit();
        self.freed = true;
        Ok(())
    }

    fn ensure_live(&self) -> Result<(), BoxError> {
        if self.freed {
            Err(BoxError::UseAfterFree)
        } else {
            Ok(())
        }
    }
}