# Implementing RFC 1951

# "msbf" - most significant bit first, Huffman encoded
# "lsbf" - least significant bit first, plain integer
# "fs" - fixed sized

# rax - bit buffer
# rbx - temporary
# rcx - temporary, memcpy len
# rdx - bit buffer bits left
# rsi - current compressed position
# rdi - current uncompressed position
# r8 - temporary
# r9  - temporary
# r10 - temporary
# r11 - temporary
# r12 - temporary
# r13 - final block flag
# r14 - value being read currently
# r15 - always 1

.macro advance_msbf_value
    shr rax, 1
    dec dl
    shlx r14, r14, r15
    adc r14, 0
.endm

.macro read_msbf_fs_field, field_size
    xor r14, r14
    .rept \field_size
        shr rax, 1
        shlx r14, r14, r15
        adc r14, 0
    .endr
    sub dl, \field_size
.endm

.macro read_msbf_field, field_size
    xor r14, r14
    mov r8, \field_size
.L_loop_\@:
    shr rax, 1
    shlx r14, r14, r15
    adc r14, 0
    dec r8
    jnz .L_loop_\@
    sub rdx, \field_size
.endm

.macro read_lsbf_fs_field, field_size
    mov r8, (\field_size << 8)
    bextr r14, rax, r8
    shr r8, 8
    shrx rax, rax, r8
    sub dl, \field_size
.endm

.macro read_lsbf_field, field_size
    xor r14, r14
    mov r8, \field_size
    shl r8, 8
    bextr r14, rax, r8
    shrx rax, rax, \field_size
    sub rdx, \field_size
.endm

.macro ensure_enough_bits
    cmp dl, 32
    jge .L_exit_\@
    test dl, dl
    jnz .L_single_word_\@
    lodsd
    mov dl, 32
.L_single_word_\@:
    mov r8, rax
    lodsd
    shlx rax, rax, rdx
    or rax, r8
    add dl, 32
.L_exit_\@:
.endm

.macro compute_prefix_counts, len_table, prefix_table, count
    mov rcx, \count
    lea r12, MVAR(len_table)
.L_loop_\@:
    movzx rbx, BYTE PTR [r12]
    test bl, bl
    jz .L_empty_\@
    inc WORD PTR IN_MVAR(prefix_table, rbx*4)
.L_empty_\@:
    inc r12
    loop .L_loop_\@
.endm

.macro compute_initial_prefix_codes_and_offsets, prefix_table, count
    xor r12, r12
    xor r11, r11
    xor r10, r10
    xor r9, r9
    mov rcx, \count
.L_loop_\@:
    mov bx, WORD PTR IN_MVAR(prefix_table, r12*4)
    test bx, bx
    jnz .L_non_empty_\@
    mov DWORD PTR IN_MVAR(prefix_table, r12*4), 0
    jmp .L_next_\@
.L_non_empty_\@:
    test r9, r9
    cmovz r9, r12
    mov WORD PTR IN_MVAR(prefix_table, r12*4 + 2), r10w
    add r10w, bx
    mov WORD PTR IN_MVAR(prefix_table, r12*4), r11w
    add r11w, bx
.L_next_\@:
    shl r11, 1
    inc r12
    loop .L_loop_\@
    dec r9w
    mov WORD PTR IN_MVAR(prefix_table, 0), r9w
.endm

.macro read_encoded_len_table, len_table, count
.ifnc \count, r9
    movzx r9, \count
.endif
    lea rbx, MVAR(len_table)
    add r9, rbx
.L_loop_\@:
    cmp rbx, r9
    jge .L_exit_\@
    ensure_enough_bits
    read_huffman_encoded_value clen_prefix_table, clen_sorted_table, 1
    cmp r14, 17
    ja .L_extra_7_bits_\@
    je .L_extra_3_bits_\@
    cmp r14, 16
    je .L_extra_2_bits_\@
    mov BYTE PTR [rbx], r14b
    inc rbx
    jb .L_loop_\@
.L_extra_7_bits_\@:
    read_lsbf_fs_field 7
    add r14, 11
    add rbx, r14
    jmp .L_loop_\@
.L_extra_3_bits_\@:
    read_lsbf_fs_field 3
    add r14, 3
    add rbx, r14
    jmp .L_loop_\@
.L_extra_2_bits_\@:
    read_lsbf_fs_field 2
    add r14, 3
    mov r11b, BYTE PTR [rbx - 1]
.L_copy_loop_\@:
    mov BYTE PTR [rbx], r11b
    inc rbx
    dec r14
    jnz .L_copy_loop_\@
    jmp .L_loop_\@
.L_exit_\@:
.endm

.macro compute_prefix_table_sorted_table, len_table, prefix_table, sorted_table, count, size
    xor r11, r11
    mov rcx, \count
.L_loop_\@:
    movzx rbx, BYTE PTR IN_MVAR(len_table, r11)
    test bl, bl
    jz .L_empty_\@
    movzx r12, WORD PTR IN_MVAR(prefix_table, rbx*4 + 2)
.ifeq \size - 1
    mov BYTE PTR IN_MVAR(sorted_table, r12), r11b
.else
    mov WORD PTR IN_MVAR(sorted_table, r12*2), r11w
.endif
    inc WORD PTR IN_MVAR(prefix_table, rbx*4 + 2)
    inc WORD PTR IN_MVAR(prefix_table, rbx*4)
.L_empty_\@:
    inc r11
    loop .L_loop_\@
.endm

.macro read_huffman_encoded_value, prefix_table, sorted_table, value_size
    movzx r11, WORD PTR MVAR(prefix_table)
    read_msbf_field r11
.L_loop_\@:
    advance_msbf_value
    inc r11
    mov r12d, DWORD PTR IN_MVAR(prefix_table, r11*4)
    test r12d, r12d
    jz .L_loop_\@
    mov r10w, r12w
    sub r10w, r14w
    jbe .L_loop_\@
    shr r12, 16
    sub r12w, r10w
    .ifeq \value_size - 1
        movzx r14, BYTE PTR IN_MVAR(sorted_table, r12)
    .else
        movzx r14, WORD PTR IN_MVAR(sorted_table, r12*2)
    .endif
.endm

.macro read_extra_bits, offsets_table, extra_bits_table, result
    movzx \result, WORD PTR [\offsets_table + r14 * 2]
    movzx r11, BYTE PTR [\extra_bits_table + r14]
    read_lsbf_field r11
    add \result, r14 
.endm

.macro memcpy
    rep movsb
.endm

.macro overlapping_memcpy src_dst_offset=r12
.ifnc \src_dst_offset,r12
    mov r12, \src_dst_offset
.endif
    cmp r12, 8
    jl .L_1_byte_\@
    cmp r12, 32
    jl .L_8_bytes_\@
    cmp rcx, 32
    jl .L_8_bytes_\@
    mov r12, rcx
    shr rcx, 5
.L_loop_\@:
    vmovupd ymm1, YMMWORD PTR [rsi]
    vmovupd YMMWORD PTR [rdi], ymm1
    add rsi, 32
    add rdi, 32
    loop .L_loop_\@
    mov rcx, r12
    and rcx, 31
.L_8_bytes_\@:
    mov r12, rcx
    shr rcx, 3
    rep movsq
    mov rcx, r12
    and rcx, 7
.L_1_byte_\@:
    rep movsb
.endm

.macro dynamic_compression
    # memset 
    mov r11, rax
    mov r12, rdi
    xor rax, rax
    lea rdi, VAR(deflate_data_start)
    lea rcx, VAR(deflate_data_end)
    sub rcx, rdi
    rep stosb
    mov rax, r11
    mov rdi, r12
    # read HLIT, HDIST and HCLEN
    ensure_enough_bits
    read_lsbf_fs_field 5
    mov BYTE PTR VAR(lit_sorted_table), r14b
    read_lsbf_fs_field 5
    mov BYTE PTR VAR(dist_sorted_table), r14b
    read_lsbf_fs_field 4

    # Read CLEN codes
    add r14, 4
    mov rbx, r14
    xor r11, r11
    lea r12, VAR(clen_len_table)
1:
    ensure_enough_bits
    read_lsbf_fs_field 3
    movzx rcx, BYTE PTR [r11 + clen_codes]
    add rcx, r12
    mov BYTE PTR [rcx], r14b
    inc r11
    cmp r11, rbx
    jne 1b

    compute_prefix_counts clen_len_table, clen_prefix_table, 19
    compute_initial_prefix_codes_and_offsets clen_prefix_table, 8
    compute_prefix_table_sorted_table clen_len_table, clen_prefix_table, clen_sorted_table, 19, 1

# Fill in lit tables
    movzx r9, BYTE PTR VAR(lit_sorted_table)
    add r9, 257
    read_encoded_len_table lit_len_table, r9
    compute_prefix_counts lit_len_table, lit_prefix_table, 286
    compute_initial_prefix_codes_and_offsets lit_prefix_table, 16
    compute_prefix_table_sorted_table lit_len_table, lit_prefix_table, lit_sorted_table, 286, 2

# Fill in dist tables
    movzx r9, BYTE PTR VAR(dist_sorted_table)
    inc r9
    read_encoded_len_table dist_len_table, r9
    compute_prefix_counts dist_len_table, dist_prefix_table, 32
    compute_initial_prefix_codes_and_offsets dist_prefix_table, 16
    compute_prefix_table_sorted_table dist_len_table, dist_prefix_table, dist_sorted_table, 32, 1

decoding_loop:
    ensure_enough_bits
    read_huffman_encoded_value lit_prefix_table, lit_sorted_table, 2
    cmp r14, 0x100
    je decoding_loop_exit
    jb literal
    sub r14, 0x100
    ensure_enough_bits
    read_extra_bits length_offsets, length_extra_bits, rcx
    ensure_enough_bits
    read_huffman_encoded_value dist_prefix_table, dist_sorted_table, 1
    ensure_enough_bits
    read_extra_bits dist_offsets, dist_extra_bits, r12
    mov r11, rsi
    mov rsi, rdi
    sub rsi, r12
    overlapping_memcpy r12
    # memcpy
    mov rsi, r11
    jmp decoding_loop
literal:
    mov BYTE PTR [rdi], r14b
    inc rdi
    jmp decoding_loop

decoding_loop_exit:

.endm

.macro deflate
    xor rdx, rdx
    mov r15, 1
    xor r13, r13
read_next_block:
    test r13, r13
    jnz exit
    ensure_enough_bits
    shr rax, 1
    adc r13, 0
    dec dl
    read_lsbf_fs_field 2
    cmp r14, 1
    jb no_compression
    je fixed_compression
    dynamic_compression
    jmp read_next_block
no_compression:
    shr rdx, 3
    sub rsi, rdx
    mov cx, WORD ptr [rsi]
    add rsi, 4
    memcpy
    xor rdx, rdx
    jmp read_next_block
fixed_compression:
    ensure_enough_bits
    read_msbf_fs_field 7
    test r14, r14
    jz read_next_block
    cmp r14, 0x17
    jbe fixed_coded_7
    advance_msbf_value
    cmp r14, 0xbf
    jbe fixed_literal_8
    cmp r14, 0xc7
    jbe fixed_coded_8
    advance_msbf_value
    sub r14, 0x190
    mov BYTE PTR [rdi], r14b
    inc rdi
    jmp fixed_compression
fixed_coded_8:
    sub r14, 0xc0
fixed_coded_7:
    read_extra_bits length_offsets, length_extra_bits, rcx
    read_msbf_fs_field 5
    read_extra_bits dist_offsets, dist_extra_bits, r12
    mov r11, rsi
    mov rsi, rdi
    sub rsi, r12
    memcpy
    mov rsi, r11
    jmp fixed_compression
fixed_literal_8:
    sub r14, 0x30
    mov BYTE PTR [rdi], r14b
    inc rdi
    jmp fixed_compression
exit:
.endm

.macro deflate_tables
length_offsets:
.dc.w 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, \
    23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
length_extra_bits:
.dc.b 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, \
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
dist_offsets:
.dc.w 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, \
    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
dist_extra_bits:
.dc.b 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, \
    10, 11, 11, 12, 12, 13, 13
clen_codes:
    .dc.b 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
.endm
