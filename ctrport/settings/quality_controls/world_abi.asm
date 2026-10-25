; SPDX-License-Identifier: GPL-2.0-only
.code
PUBLIC quality_sysv_nonvolatile_probe
quality_sysv_nonvolatile_probe PROC
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov [rsp],rdi
 mov [rsp+8],rcx
 mov [rsp+16],rsp
 mov qword ptr [rsp+24],12345678h
 mov rdi,rsi
 mov rsi,rdx
 mov rbp,1111h
 mov rbx,2222h
 mov r12,3333h
 mov r13,4444h
 mov r14,5555h
 mov r15,6666h
 call qword ptr [rsp]
 mov rax,[rsp+8]
 mov [rax],rbp
 mov [rax+8],rbx
 mov [rax+16],r12
 mov [rax+24],r13
 mov [rax+32],r14
 mov [rax+40],r15
 mov rdx,rsp
 sub rdx,[rsp+16]
 mov [rax+48],rdx
 mov rdx,[rsp+24]
 mov [rax+56],rdx
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
quality_sysv_nonvolatile_probe ENDP
END
