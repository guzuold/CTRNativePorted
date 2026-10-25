; SPDX-License-Identifier: GPL-2.0-only
EXTERN ctr_settings_host_event:PROC
EXTERN ctr_settings_host_index:PROC
.code
PUBLIC ctr_settings_event_thunk
ctr_settings_event_thunk PROC
    sub rsp, 40
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_settings_host_event
    add rsp, 40
    ret
ctr_settings_event_thunk ENDP
PUBLIC ctr_settings_index_thunk
ctr_settings_index_thunk PROC
    sub rsp, 40
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_settings_host_index
    add rsp, 40
    ret
ctr_settings_index_thunk ENDP
; Win64(fn,a,b,c,d) -> SysV fn(a,b,c,d). Win64 nonvolatile SIMD
; registers are caller-saved under SysV and must be preserved here.
PUBLIC ctr_settings_guest_call4
ctr_settings_guest_call4 PROC
    push rdi
    push rsi
    sub rsp, 168
    movdqu [rsp], xmm6
    movdqu [rsp+16], xmm7
    movdqu [rsp+32], xmm8
    movdqu [rsp+48], xmm9
    movdqu [rsp+64], xmm10
    movdqu [rsp+80], xmm11
    movdqu [rsp+96], xmm12
    movdqu [rsp+112], xmm13
    movdqu [rsp+128], xmm14
    movdqu [rsp+144], xmm15
    mov rax, rcx
    mov rdi, rdx
    mov rsi, r8
    mov rdx, r9
    mov rcx, [rsp+224]
    call rax
    movdqu xmm6, [rsp]
    movdqu xmm7, [rsp+16]
    movdqu xmm8, [rsp+32]
    movdqu xmm9, [rsp+48]
    movdqu xmm10, [rsp+64]
    movdqu xmm11, [rsp+80]
    movdqu xmm12, [rsp+96]
    movdqu xmm13, [rsp+112]
    movdqu xmm14, [rsp+128]
    movdqu xmm15, [rsp+144]
    add rsp, 168
    pop rsi
    pop rdi
    ret
ctr_settings_guest_call4 ENDP
END
