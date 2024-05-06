.macro sys_call idx=rax, a=rdi, b=rsi, c=rdx, d=r10, e=r8, f=r9
.ifnc \f,r9
    mov r9, \f
.endif
.ifnc \e,r8
    mov r8, \e
.endif
.ifnc \d,r10
    mov r10, \d
.endif
.ifnc \c,rdx
    mov rdx, \c
.endif
.ifnc \b,rsi
    mov rsi, \b
.endif
.ifnc \a,rdi
    mov rdi, \a
.endif
.ifnc \idx,rax
    mov rax, \idx
.endif
    syscall
.endm

.macro str_lit name, value
\()\name:
    .ascii "\value"
    .set \()\name\()_len, . - \()\name
.endm

.macro begin_frame
    mov rbp, rsp
    .set locals_offset, 0
.endm

.macro local_var name, size
.ifndef locals_offset
    .set locals_offset, \size
.else
    .set locals_offset, locals_offset + \size
.endif
    .set \name\()_offset, locals_offset
.endm

.macro end_frame 
    sub rsp, locals_offset
.endm

#define VAR(name) [rbp - name##_offset]
#define IN_VAR(name, in_offset) [rbp - name##_offset + in_offset]

.macro breakpoint
    int 3
.endm
