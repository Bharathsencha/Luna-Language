// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use std::collections::HashMap;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct AllocMeta {
    pub size: usize,
    pub poisoned: bool,
    pub base: usize,
}

#[derive(Default)]
pub struct AllocTable {
    entries: HashMap<usize, AllocMeta>,
}

impl AllocTable {
    pub fn clear(&mut self) {
        self.entries.clear();
    }

    pub fn register(&mut self, ptr: usize, size: usize) {
        self.entries.insert(
            ptr,
            AllocMeta {
                size,
                poisoned: false,
                base: ptr,
            },
        );
    }

    pub fn get_exact(&self, ptr: usize) -> Option<&AllocMeta> {
        self.entries.get(&ptr)
    }

    pub fn get_exact_mut(&mut self, ptr: usize) -> Option<&mut AllocMeta> {
        self.entries.get_mut(&ptr)
    }

    pub fn find_owner(&self, ptr: usize) -> Option<(usize, &AllocMeta)> {
        self.entries.iter().find_map(|(base, meta)| {
            let end = base.saturating_add(meta.size);
            if ptr >= *base && ptr < end {
                Some((*base, meta))
            } else {
                None
            }
        })
    }

    pub fn contains_pointer_value(&self, ptr: usize) -> bool {
        self.find_owner(ptr).is_some()
    }

    pub fn leak_count(&self) -> usize {
        self.entries.values().filter(|meta| !meta.poisoned).count()
    }
}