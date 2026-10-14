; SPDX-License-Identifier: GPL-2.0-or-later
; Synthetic ABI linkage control, not retail code or a PS4 service.
EXTERN ctr_msvc_probe:PROC
EXTERN ctr_sysv_orbis_condition_broadcast:PROC
.code
PUBLIC ctr_sysv_probe
ctr_sysv_probe PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rcx, rdi
    mov rdx, rsi
    call ctr_msvc_probe
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_sysv_probe ENDP
PUBLIC ctr_call_sysv_probe
ctr_call_sysv_probe PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov rsi, rdx
    call ctr_sysv_probe
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_call_sysv_probe ENDP
; Startup stack/register design adapted from shadPS4 linker.cpp RunMainEntry.
; Revision and source SHA recorded in ctrport_startup_abi.json.
PUBLIC ctr_jump_startup
PUBLIC ctr_jump_startup_owned
; Same nonreturning startup ABI, with committed guest stack and matching TEB bounds.
ctr_jump_startup_owned PROC
    mov r10, [rsp+40]
    mov gs:[8], r9
    mov gs:[10h], r10
    mov r11, rsp
    mov rsp, r9
    and rsp, -16
    sub rsp, 8
    push qword ptr [rdx+8]
    push qword ptr [rdx]
    mov rdi, rdx
    mov rsi, r8
    jmp rcx
ctr_jump_startup_owned ENDP
ctr_jump_startup PROC
    mov r11, rsp
    and rsp, -16
    sub rsp, 8
    push qword ptr [rdx+8]
    push qword ptr [rdx]
    mov rdi, rdx
    mov rsi, r8
    jmp rcx
ctr_jump_startup ENDP
; Synthetic target alone restores the saved host stack; retail never returns here.
ctr_startup_target PROC
    mov rax, rsp
    and rax, 15
    mov qword ptr [r9], rax
    mov rax, qword ptr [rsp]
    mov qword ptr [r9+8], rax
    mov rax, qword ptr [rsp+8]
    mov qword ptr [r9+16], rax
    mov qword ptr [r9+24], rdi
    mov qword ptr [r9+32], rsi
    mov rsp, r11
    ret
ctr_startup_target ENDP
PUBLIC ctr_startup_control
ctr_startup_control PROC
    push rdi
    push rsi
    sub rsp, 40
    mov r9, r8
    mov r8, rdx
    mov rdx, rcx
    lea rcx, ctr_startup_target
    call ctr_jump_startup
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_startup_control ENDP
; Win64 nonvolatile XMM6..15 may be clobbered by original SysV code.
PUBLIC ctr_original_integer_target_control
ctr_original_integer_target_control PROC
    mov rax, rdi
    xor rax, rsi
    pxor xmm6, xmm6
    pxor xmm7, xmm7
    pxor xmm8, xmm8
    pxor xmm9, xmm9
    pxor xmm10, xmm10
    pxor xmm11, xmm11
    pxor xmm12, xmm12
    pxor xmm13, xmm13
    pxor xmm14, xmm14
    pxor xmm15, xmm15
    ret
ctr_original_integer_target_control ENDP
PUBLIC ctr_call_original_integer2
ctr_call_original_integer2 PROC
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
    mov rdi, rdx
    mov rsi, r8
    call rcx
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
ctr_call_original_integer2 ENDP
PUBLIC ctr_original_xmm_control
ctr_original_xmm_control PROC
    sub rsp, 200
    movdqu [rsp+32], xmm6
    movdqu [rsp+48], xmm7
    movdqu [rsp+64], xmm8
    movdqu [rsp+80], xmm9
    movdqu [rsp+96], xmm10
    movdqu [rsp+112], xmm11
    movdqu [rsp+128], xmm12
    movdqu [rsp+144], xmm13
    movdqu [rsp+160], xmm14
    movdqu [rsp+176], xmm15
    pcmpeqd xmm6, xmm6
    pcmpeqd xmm7, xmm7
    pcmpeqd xmm8, xmm8
    pcmpeqd xmm9, xmm9
    pcmpeqd xmm10, xmm10
    pcmpeqd xmm11, xmm11
    pcmpeqd xmm12, xmm12
    pcmpeqd xmm13, xmm13
    pcmpeqd xmm14, xmm14
    pcmpeqd xmm15, xmm15
    lea rcx, ctr_original_integer_target_control
    xor edx, edx
    xor r8d, r8d
    call ctr_call_original_integer2
    mov r10d, 1
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm6
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm7
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm8
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm9
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm10
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm11
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm12
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm13
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm14
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm15
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    movdqu xmm6, [rsp+32]
    movdqu xmm7, [rsp+48]
    movdqu xmm8, [rsp+64]
    movdqu xmm9, [rsp+80]
    movdqu xmm10, [rsp+96]
    movdqu xmm11, [rsp+112]
    movdqu xmm12, [rsp+128]
    movdqu xmm13, [rsp+144]
    movdqu xmm14, [rsp+160]
    movdqu xmm15, [rsp+176]
    mov eax, r10d
    add rsp, 200
    ret
ctr_original_xmm_control ENDP
PUBLIC ctr_call_original_integer3
ctr_call_original_integer3 PROC
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
    mov rdi, rdx
    mov rsi, r8
    mov rdx, r9
    call rcx
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
ctr_call_original_integer3 ENDP

PUBLIC ctr_sysv_integer3_fixture
ctr_sysv_integer3_fixture PROC
    mov [rdi], rsi
    mov [rdi+8], rdx
    mov rax, rdx
    ret
ctr_sysv_integer3_fixture ENDP

PUBLIC ctr_call_original_registers
ctr_call_original_registers PROC
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
    mov [rsp+160], r9
    mov r11, rdx
    mov r10, rcx
    movq xmm0, qword ptr [r8]
    movq xmm1, qword ptr [r8+8]
    movq xmm2, qword ptr [r8+16]
    movq xmm3, qword ptr [r8+24]
    movq xmm4, qword ptr [r8+32]
    movq xmm5, qword ptr [r8+40]
    movq xmm6, qword ptr [r8+48]
    movq xmm7, qword ptr [r8+56]
    mov rdi, [r11]
    mov rsi, [r11+8]
    mov rdx, [r11+16]
    mov rcx, [r11+24]
    mov r8, [r11+32]
    mov r9, [r11+40]
    call r10
    mov r11, [rsp+160]
    movq qword ptr [r11], xmm0
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
ctr_call_original_registers ENDP
PUBLIC ctr_sysv_register_fixture
ctr_sysv_register_fixture PROC
    mov [rdi], rdi
    mov [rdi+8], rsi
    mov [rdi+16], rdx
    mov [rdi+24], rcx
    mov [rdi+32], r8
    mov [rdi+40], r9
    movq qword ptr [rdi+48], xmm0
    movq qword ptr [rdi+56], xmm1
    movq qword ptr [rdi+64], xmm2
    movq qword ptr [rdi+72], xmm3
    movq qword ptr [rdi+80], xmm4
    movq qword ptr [rdi+88], xmm5
    movq qword ptr [rdi+96], xmm6
    movq qword ptr [rdi+104], xmm7
    movaps xmm0, xmm7
    mov rax, r9
    ret
ctr_sysv_register_fixture ENDP
PUBLIC ctr_original_register_preservation_control
ctr_original_register_preservation_control PROC
    push rdi
    push rsi
    sub rsp, 344
    movdqu [rsp+32], xmm6
    movdqu [rsp+48], xmm7
    movdqu [rsp+64], xmm8
    movdqu [rsp+80], xmm9
    movdqu [rsp+96], xmm10
    movdqu [rsp+112], xmm11
    movdqu [rsp+128], xmm12
    movdqu [rsp+144], xmm13
    movdqu [rsp+160], xmm14
    movdqu [rsp+176], xmm15
    pcmpeqd xmm6, xmm6
    pcmpeqd xmm7, xmm7
    pcmpeqd xmm8, xmm8
    pcmpeqd xmm9, xmm9
    pcmpeqd xmm10, xmm10
    pcmpeqd xmm11, xmm11
    pcmpeqd xmm12, xmm12
    pcmpeqd xmm13, xmm13
    pcmpeqd xmm14, xmm14
    pcmpeqd xmm15, xmm15
    mov rdi, 12345
    mov rsi, 54321
    lea rcx, ctr_original_integer_target_control
    lea rdx, [rsp+192]
    lea r8, [rsp+240]
    lea r9, [rsp+304]
    call ctr_call_original_registers
    xor r10d, r10d
    cmp rdi, 12345
    jne preservation_integer_done
    cmp rsi, 54321
    jne preservation_integer_done
    mov r10d, 1
preservation_integer_done:
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm6
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm7
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm8
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm9
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm10
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm11
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm12
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm13
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm14
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm15
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    movdqu xmm6, [rsp+32]
    movdqu xmm7, [rsp+48]
    movdqu xmm8, [rsp+64]
    movdqu xmm9, [rsp+80]
    movdqu xmm10, [rsp+96]
    movdqu xmm11, [rsp+112]
    movdqu xmm12, [rsp+128]
    movdqu xmm13, [rsp+144]
    movdqu xmm14, [rsp+160]
    movdqu xmm15, [rsp+176]
    mov eax, r10d
    add rsp, 344
    pop rsi
    pop rdi
    ret
ctr_original_register_preservation_control ENDP
; Independent Win64 -> SysV owned-stack call. Publish actual TEB stack bounds.
; No general C++ unwind across this stack switch.
PUBLIC ctr_guest_stack_call
ctr_guest_stack_call PROC
    push rdi
    push rsi
    push r12
    sub rsp, 192
    movdqu [rsp+0], xmm6
    movdqu [rsp+16], xmm7
    movdqu [rsp+32], xmm8
    movdqu [rsp+48], xmm9
    movdqu [rsp+64], xmm10
    movdqu [rsp+80], xmm11
    movdqu [rsp+96], xmm12
    movdqu [rsp+112], xmm13
    movdqu [rsp+128], xmm14
    movdqu [rsp+144], xmm15
    mov r10, gs:[08h]
    mov [rsp+160], r10
    mov r10, gs:[10h]
    mov [rsp+168], r10
    mov gs:[08h], r8
    mov gs:[10h], r9
    stmxcsr [rsp+176]
    fnstcw [rsp+180]
    mov dword ptr [rsp+184], 09fc0h
    mov word ptr [rsp+188], 037fh
    ldmxcsr [rsp+184]
    fldcw [rsp+188]
    mov r12, rsp
    mov rdi, rdx
    xor esi, esi
    mov rsp, r8
    and rsp, -16
    call rcx
    mov rsp, r12
    mov r10, [rsp+160]
    mov gs:[08h], r10
    mov r10, [rsp+168]
    mov gs:[10h], r10
    movdqu xmm6, [rsp+0]
    movdqu xmm7, [rsp+16]
    movdqu xmm8, [rsp+32]
    movdqu xmm9, [rsp+48]
    movdqu xmm10, [rsp+64]
    movdqu xmm11, [rsp+80]
    movdqu xmm12, [rsp+96]
    movdqu xmm13, [rsp+112]
    movdqu xmm14, [rsp+128]
    movdqu xmm15, [rsp+144]
    ldmxcsr [rsp+176]
    fldcw [rsp+180]
    add rsp, 192
    pop r12
    pop rsi
    pop rdi
    ret
ctr_guest_stack_call ENDP
; Guest thread entry used by GuestWorkerControl: as ctr_guest_stack_call, and also saves
; rbx, rbp, r13-r15 and publishes the frame in *frame_slot (5th argument) so that
; scePthreadExit (ctr_sysv_pthread_exit) can return from the guest thread at any depth
; with its exit value, exactly as if the entry function had returned that value.
; Frame: 8 pushes, then 200 bytes; the 5th argument is at [frame+304].
PUBLIC ctr_guest_thread_call
ctr_guest_thread_call PROC
    push rdi
    push rsi
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    sub rsp, 200
    movdqu [rsp+0], xmm6
    movdqu [rsp+16], xmm7
    movdqu [rsp+32], xmm8
    movdqu [rsp+48], xmm9
    movdqu [rsp+64], xmm10
    movdqu [rsp+80], xmm11
    movdqu [rsp+96], xmm12
    movdqu [rsp+112], xmm13
    movdqu [rsp+128], xmm14
    movdqu [rsp+144], xmm15
    mov r10, gs:[08h]
    mov [rsp+160], r10
    mov r10, gs:[10h]
    mov [rsp+168], r10
    mov gs:[08h], r8
    mov gs:[10h], r9
    stmxcsr [rsp+176]
    fnstcw [rsp+180]
    mov dword ptr [rsp+184], 09fc0h
    mov word ptr [rsp+188], 037fh
    ldmxcsr [rsp+184]
    fldcw [rsp+188]
    mov r12, rsp
    mov r10, [rsp+304]
    mov [r10], r12
    mov rdi, rdx
    xor esi, esi
    mov rsp, r8
    and rsp, -16
    call rcx
    mov rsp, r12
ctr_guest_thread_return::
    mov r10, [rsp+304]
    mov qword ptr [r10], 0
    mov r10, [rsp+160]
    mov gs:[08h], r10
    mov r10, [rsp+168]
    mov gs:[10h], r10
    movdqu xmm6, [rsp+0]
    movdqu xmm7, [rsp+16]
    movdqu xmm8, [rsp+32]
    movdqu xmm9, [rsp+48]
    movdqu xmm10, [rsp+64]
    movdqu xmm11, [rsp+80]
    movdqu xmm12, [rsp+96]
    movdqu xmm13, [rsp+112]
    movdqu xmm14, [rsp+128]
    movdqu xmm15, [rsp+144]
    ldmxcsr [rsp+176]
    fldcw [rsp+180]
    add rsp, 200
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop rsi
    pop rdi
    ret
ctr_guest_thread_call ENDP
; scePthreadExit(void* value), SysV, called by guest code on a GuestWorkerControl thread.
; ctr_thread_exit_frame(value, caller) logs the exit and returns the published frame, or
; reports the unsupported call (no worker frame) and ends the process.
EXTERN ctr_thread_exit_frame:PROC
PUBLIC ctr_sysv_pthread_exit
ctr_sysv_pthread_exit PROC
    mov r15, rdi
    mov rdx, [rsp]
    mov rcx, rdi
    and rsp, -16
    sub rsp, 32
    call ctr_thread_exit_frame
    mov rsp, rax
    mov rax, r15
    jmp ctr_guest_thread_return
ctr_sysv_pthread_exit ENDP
; Control fixture (SysV, rdi = exit value): overwrites every callee-saved register and
; exits from a nested call, so the host thread must get the value and its own registers back.
PUBLIC ctr_thread_exit_fixture
ctr_thread_exit_fixture PROC
    push rbp
    mov rbx, 1111h
    mov rbp, 2222h
    mov r12, 3333h
    mov r13, 4444h
    mov r14, 5555h
    mov r15, 6666h
    call ctr_thread_exit_fixture_inner
    xor eax, eax
    pop rbp
    ret
ctr_thread_exit_fixture ENDP
ctr_thread_exit_fixture_inner PROC
    sub rsp, 8
    call ctr_sysv_pthread_exit
    add rsp, 8
    ret
ctr_thread_exit_fixture_inner ENDP
PUBLIC ctr_guest_stack_fixture
ctr_guest_stack_fixture PROC
    mov [rdi], rsp
    mov qword ptr [rsp-128], 17
    mov qword ptr [rsp-8], 23
    mov rax, [rsp-128]
    add rax, [rsp-8]
    pxor xmm6, xmm6
    pxor xmm7, xmm7
    pxor xmm8, xmm8
    pxor xmm9, xmm9
    pxor xmm10, xmm10
    pxor xmm11, xmm11
    pxor xmm12, xmm12
    pxor xmm13, xmm13
    pxor xmm14, xmm14
    pxor xmm15, xmm15
    ret
ctr_guest_stack_fixture ENDP
PUBLIC ctr_guest_stack_xmm_control
ctr_guest_stack_xmm_control PROC
    mov r11, r8
    mov r9, rcx
    sub rsp, 200
    movdqu [rsp+32], xmm6
    movdqu [rsp+48], xmm7
    movdqu [rsp+64], xmm8
    movdqu [rsp+80], xmm9
    movdqu [rsp+96], xmm10
    movdqu [rsp+112], xmm11
    movdqu [rsp+128], xmm12
    movdqu [rsp+144], xmm13
    movdqu [rsp+160], xmm14
    movdqu [rsp+176], xmm15
    pcmpeqd xmm6, xmm6
    pcmpeqd xmm7, xmm7
    pcmpeqd xmm8, xmm8
    pcmpeqd xmm9, xmm9
    pcmpeqd xmm10, xmm10
    pcmpeqd xmm11, xmm11
    pcmpeqd xmm12, xmm12
    pcmpeqd xmm13, xmm13
    pcmpeqd xmm14, xmm14
    pcmpeqd xmm15, xmm15
    lea rcx, ctr_guest_stack_fixture
    mov r8, r9
    mov r9, r11
    call ctr_guest_stack_call
    mov r10d, 1
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm6
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm7
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm8
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm9
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm10
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm11
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm12
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm13
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm14
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    pcmpeqd xmm0, xmm0
    pcmpeqb xmm0, xmm15
    pmovmskb eax, xmm0
    cmp eax, 65535
    sete al
    movzx eax, al
    and r10d, eax
    movdqu xmm6, [rsp+32]
    movdqu xmm7, [rsp+48]
    movdqu xmm8, [rsp+64]
    movdqu xmm9, [rsp+80]
    movdqu xmm10, [rsp+96]
    movdqu xmm11, [rsp+112]
    movdqu xmm12, [rsp+128]
    movdqu xmm13, [rsp+144]
    movdqu xmm14, [rsp+160]
    movdqu xmm15, [rsp+176]
    mov eax, r10d
    add rsp, 200
    ret
ctr_guest_stack_xmm_control ENDP
PUBLIC ctr_guest_stack_exception_fixture
PUBLIC ctr_guest_stack_exception_site
ctr_guest_stack_exception_fixture PROC
    mov qword ptr [rsp-128], 17
    mov qword ptr [rsp-8], 23
ctr_guest_stack_exception_site LABEL BYTE
    ud2
    mov rax, [rsp-128]
    add rax, [rsp-8]
    ret
ctr_guest_stack_exception_fixture ENDP
PUBLIC ctr_worker_missing_fixture
PUBLIC ctr_worker_missing_caller
ctr_worker_missing_fixture PROC
    sub rsp, 8
    call ctr_sysv_orbis_condition_broadcast
ctr_worker_missing_caller LABEL BYTE
    add rsp, 8
    ret
ctr_worker_missing_fixture ENDP
PUBLIC ctr_guest_fp_fixture
ctr_guest_fp_fixture PROC
    stmxcsr [rdi]
    fnstcw [rdi+4]
    xor eax, eax
    ret
ctr_guest_fp_fixture ENDP
PUBLIC ctr_host_fp_snapshot
ctr_host_fp_snapshot PROC
    stmxcsr [rcx]
    fnstcw [rcx+4]
    ret
ctr_host_fp_snapshot ENDP
END
