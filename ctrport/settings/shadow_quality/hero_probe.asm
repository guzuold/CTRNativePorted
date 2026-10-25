; SPDX-License-Identifier: GPL-2.0-only
.code
probe_hero PROC
 push rdi
 push rsi
 sub rsp,40
 mov rdi,rdx
 mov rsi,r8
 call rcx
 add rsp,40
 pop rsi
 pop rdi
 ret
probe_hero ENDP
END
