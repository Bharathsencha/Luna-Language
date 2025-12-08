; time.asm
; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (c) 2025 Bharath

global get_monotonic_time

section .text

; void get_monotonic_time(struct timespec *ts)
; RDI contains the pointer to the timespec struct
get_monotonic_time:
    ; We need to make syscall 228 (sys_clock_gettime)
    ; Arguments for sys_clock_gettime:
    ; RDI: clock_id (1 = CLOCK_MONOTONIC)
    ; RSI: struct timespec *tp
    
    mov rsi, rdi    ; Move the struct pointer (currently in RDI) to RSI (2nd arg)
    mov rdi, 1      ; 1 = CLOCK_MONOTONIC (counts up from boot, good for measuring duration)
    mov rax, 228    ; 228 = sys_clock_gettime on x86_64 Linux
    syscall         ; Invoke the kernel
    
    ret             ; Return to C