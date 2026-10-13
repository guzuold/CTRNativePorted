; SPDX-License-Identifier: GPL-2.0-only
; Signature1 only: three integer arguments, integer result. Not a generic trap.
EXTERN ctr_waiting_record_integer9:PROC
EXTERN ctr_waiting_integer3:PROC
EXTERN ctr_waiting_record_integer3:PROC
.code
PUBLIC ctr_sysv_waiting_integer3
ctr_sysv_waiting_integer3 PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    mov r9, qword ptr [rsp+40]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    call ctr_waiting_integer3
    add rsp, 40
    ret
ctr_sysv_waiting_integer3 ENDP
PUBLIC ctr_sysv_record_integer3
ctr_sysv_record_integer3 PROC FRAME
    sub rsp, 56
    .allocstack 56
    .endprolog
    mov rax, qword ptr [rsp+56]
    mov qword ptr [rsp+32], rax
    mov r9, rdx
    mov r8, rsi
    mov rdx, rdi
    mov rcx, r11
    call ctr_waiting_record_integer3
    add rsp, 56
    ret
ctr_sysv_record_integer3 ENDP
PUBLIC ctr_sysv_record_integer9
ctr_sysv_record_integer9 PROC FRAME
    sub rsp, 120
    .allocstack 120
    .endprolog
    mov qword ptr [rsp+32], rdi
    mov qword ptr [rsp+40], rsi
    mov qword ptr [rsp+48], rdx
    mov qword ptr [rsp+56], rcx
    mov qword ptr [rsp+64], r8
    mov qword ptr [rsp+72], r9
    mov rax, qword ptr [rsp+128]
    mov qword ptr [rsp+80], rax
    mov rax, qword ptr [rsp+136]
    mov qword ptr [rsp+88], rax
    mov rax, qword ptr [rsp+144]
    mov qword ptr [rsp+96], rax
    mov r8, qword ptr [rsp+120]
    lea rdx, [rsp+32]
    mov rcx, r11
    call ctr_waiting_record_integer9
    add rsp, 120
    ret
ctr_sysv_record_integer9 ENDP
PUBLIC ctr_service_integer9_fixture
ctr_service_integer9_fixture PROC FRAME
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    sub rsp, 40
    .allocstack 40
    .endprolog
    mov r10, rcx
    mov rdi, qword ptr [rdx]
    mov rsi, qword ptr [rdx+8]
    mov rcx, qword ptr [rdx+24]
    mov r8, qword ptr [rdx+32]
    mov r9, qword ptr [rdx+40]
    mov rax, qword ptr [rdx+48]
    mov qword ptr [rsp], rax
    mov rax, qword ptr [rdx+56]
    mov qword ptr [rsp+8], rax
    mov rax, qword ptr [rdx+64]
    mov qword ptr [rsp+16], rax
    mov rdx, qword ptr [rdx+16]
    call r10
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_service_integer9_fixture ENDP
END
