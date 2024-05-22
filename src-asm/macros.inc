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
#define MVAR(name) [rbp - \name\()_offset]
#define IN_VAR(name, in_offset) [rbp - name##_offset + in_offset]
#define IN_MVAR(name, in_offset) [rbp - \name\()_offset + in_offset]

.macro breakpoint
    int 3
.endm
