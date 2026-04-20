// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

use crate::error;

#[derive(Default)]
pub struct BlockState {
    inside_unsafe: bool,
}

impl BlockState {
    pub fn enter(&mut self) -> i32 {
        if self.inside_unsafe {
            error::ERR_NESTED_UNSAFE
        } else {
            self.inside_unsafe = true;
            error::OK
        }
    }

    pub fn exit(&mut self) {
        self.inside_unsafe = false;
    }
}