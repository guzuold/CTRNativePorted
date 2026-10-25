; SPDX-License-Identifier: GPL-2.0-only
option casemap:none
.code
; Test-only continuation harness for the original bounded producer block.
; No fabricated helper result or original function body is used as an oracle.
vfx_producer_block PROC FRAME
 push rbp
 .pushreg rbp
 push rbx
 .pushreg rbx
 push rsi
 .pushreg rsi
 push rdi
 .pushreg rdi
 push r12
 .pushreg r12
 push r14
 .pushreg r14
 push r15
 .pushreg r15
 sub rsp,70h
 .allocstack 70h
 .endprolog
 lea rbp,[rsp+70h]
 mov r12,rdx
 mov r15,r8
 mov [rbp-50h],rdx
 call rcx
 add rsp,70h
 pop r15
 pop r14
 pop r12
 pop rdi
 pop rsi
 pop rbx
 pop rbp
 ret
vfx_producer_block ENDP
END
