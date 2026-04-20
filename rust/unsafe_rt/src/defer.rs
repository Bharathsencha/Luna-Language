// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#[derive(Default)]
pub struct DeferStack {
    values: Vec<usize>,
}

impl DeferStack {
    pub fn clear(&mut self) {
        self.values.clear();
    }

    pub fn push(&mut self, ptr: usize) {
        self.values.push(ptr);
    }

    pub fn pop(&mut self) -> Option<usize> {
        self.values.pop()
    }
}