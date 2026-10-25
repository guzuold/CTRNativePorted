; SPDX-License-Identifier: GPL-2.0-only
.code
probe_config PROC
 push rbx
 push r13
 sub rsp,40
 mov rbx,rdx
 mov r13,r8
 mov eax,r9d
 vcvtsi2ss xmm0,xmm0,dword ptr [rbx+1f0h]
 call rcx
 add rsp,40
 pop r13
 pop rbx
 ret
probe_config ENDP
probe_publish PROC
 push rbx
 push r15
 push rbp
 sub rsp,400h
 lea rbp,[rsp+300h]
 mov r15,rdx
 mov rbx,r8
 call rcx
 add rsp,400h
 pop rbp
 pop r15
 pop rbx
 ret
probe_publish ENDP
probe_viewport PROC
 push rbx
 push r12
 push r13
 push rsi
 sub rsp,40
 mov r13,rdx
 mov r12,r8
 mov ebx,r9d
 call rcx
 add rsp,40
 pop rsi
 pop r13
 pop r12
 pop rbx
 ret
probe_viewport ENDP
END
