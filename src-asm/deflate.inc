# Implementing RFC 1951

# "msbf" - most significant bit first, Huffman encoded
# "lsbf" - least significant bit first, plain integer
# "fs" - fixed sized

# Global state:
# rax - bit buffer
# rbx - value being currently read
# rdx - bit buffer bits left
# rsi - current compressed position
# rdi - current uncompressed position

# Local variables: r8-r15

#define PREFIX_LENGTH(table, reg) WORD PTR IN_MVAR(table, reg*2)
#define PREFIX_CODE(table, reg) WORD PTR IN_MVAR(table, reg*2)
#define PREFIX_OFFSET(table, reg) WORD PTR IN_MVAR(table, reg*2 + 32)
#define PREFIX_FIRST_NONEMPTY(table) WORD PTR MVAR(table)

.macro advance_msbf_value
    shr rax, 1
    rcl rbx, 1
    dec dl
.endm

.macro read_msbf_fs_field, field_size
    xor rbx, rbx
    .rept \field_size
        shr rax, 1
        rcl rbx, 1
    .endr
    sub dl, \field_size
.endm

.macro read_msbf_field, field_size
    xor rbx, rbx
    mov r8, \field_size
.L_loop_\@:
    shr rax, 1
    rcl rbx, 1
    dec r8
    jnz .L_loop_\@
    sub rdx, \field_size
.endm

.macro read_lsbf_fs_field, field_size
    mov r8, (\field_size << 8)
    bextr rbx, rax, r8
    shr r8, 8
    shrx rax, rax, r8
    sub dl, \field_size
.endm

.macro read_lsbf_field, field_size
    xor rbx, rbx
    mov r8, \field_size
    shl r8, 8
    bextr rbx, rax, r8
    shrx rax, rax, \field_size
    sub rdx, \field_size
.endm

.macro ensure_enough_bits
    cmp dl, 32
    jg .L_exit_\@
    mov r8, rax
    lodsd
    shlx rax, rax, rdx
    or rax, r8
    add dl, 32
.L_exit_\@:
.endm

.macro compute_prefix_counts, prefix_table, count
    mov rcx, \count
    lea r8, VAR(len_table)
    lea r9, [r8 + \count]
.L_loop_\@:
    movzx r10, BYTE PTR [r8]
    test r10b, r10b
    jz .L_empty_\@
    inc PREFIX_LENGTH(prefix_table, r10)
.L_empty_\@:
    inc r8
    cmp r8, r9
    jne .L_loop_\@
.endm

.macro compute_initial_prefix_codes_and_offsets, prefix_table, count
    xor r8, r8 # Current prefix length
    xor r9, r9 # Min non-empty prefix length
    xor r10, r10 # Curent prefix offset
    xor r11, r11 # Current prefix code
.L_loop_\@:
    mov r12w, PREFIX_LENGTH(prefix_table, r8)
    test r12w, r12w
    jnz .L_non_empty_\@
    mov PREFIX_OFFSET(prefix_table, r8), 0
    jmp .L_next_\@
.L_non_empty_\@:
    test r9, r9
    cmovz r9, r8
    mov PREFIX_OFFSET(prefix_table, r8), r10w
    add r10w, r12w
    mov PREFIX_CODE(prefix_table, r8), r11w
    add r11w, r12w
.L_next_\@:
    shl r11, 1
    inc r8
    cmp r8, \count
    jne .L_loop_\@
    mov PREFIX_FIRST_NONEMPTY(prefix_table), r9w
.endm

.macro read_encoded_len_table, count
    xor r11, r11
.L_loop_\@:
    cmp r11, \count
    jge .L_exit_\@
    ensure_enough_bits
    read_huffman_encoded_value clen_prefix_table, clen_sorted_table, 1
    cmp rbx, 17
    ja .L_extra_7_bits_\@
    je .L_extra_3_bits_\@
    cmp rbx, 16
    je .L_extra_2_bits_\@
    mov BYTE PTR IN_VAR(len_table, r11), bl
    inc r11
    jb .L_loop_\@
.L_extra_7_bits_\@:
    read_lsbf_fs_field 7
    add rbx, 11
    mov r8b, 0
    jmp .L_copy_loop_\@
.L_extra_3_bits_\@:
    read_lsbf_fs_field 3
    add rbx, 3
    mov r8b, 0
    jmp .L_copy_loop_\@
.L_extra_2_bits_\@:
    read_lsbf_fs_field 2
    add rbx, 3
    mov r8b, BYTE PTR IN_VAR(len_table, r11 - 1)
.L_copy_loop_\@:
    mov BYTE PTR IN_VAR(len_table, r11), r8b
    inc r11
    dec rbx
    jnz .L_copy_loop_\@
    jmp .L_loop_\@
.L_exit_\@:
.endm

.macro compute_prefix_table_sorted_table, prefix_table, sorted_table, count, size
    xor r8, r8
.L_loop_\@:
    movzx r9, BYTE PTR IN_VAR(len_table, r8)
    test r9, r9
    jz .L_empty_\@
    movzx r10, PREFIX_OFFSET(prefix_table, r9)
.ifeq \size - 1
    mov BYTE PTR IN_MVAR(sorted_table, r10), r8b
.else
    mov WORD PTR IN_MVAR(sorted_table, r10*2), r8w
.endif
    inc PREFIX_OFFSET(prefix_table, r9)
    inc PREFIX_CODE(prefix_table, r9)
.L_empty_\@:
    inc r8
    cmp r8, \count
    jne .L_loop_\@
.endm

.macro read_huffman_encoded_value, prefix_table, sorted_table, value_size
    prefetchw IN_MVAR(sorted_table, 0)
    movzx r9, PREFIX_FIRST_NONEMPTY(prefix_table) # Start with first not empty prefix
    read_msbf_field r9

    .rept 4
        mov r10, QWORD PTR IN_MVAR(prefix_table, r9*2)
        .rept 4
            sub r10w, bx
            ja .L_found_\@
            advance_msbf_value
            inc r9
            shr r10, 16
        .endr 
    .endr

.L_found_\@:
    movzx r8, PREFIX_OFFSET(prefix_table, r9)
    sub r8w, r10w
    .ifeq \value_size - 1
        movzx rbx, BYTE PTR IN_MVAR(sorted_table, r8)
    .else
        movzx rbx, WORD PTR IN_MVAR(sorted_table, r8*2)
    .endif
.endm

.macro read_extra_bits, code, offsets_table, extra_bits_table, result
    movzx \result, WORD PTR [\offsets_table + \code * 2]
    movzx r9, BYTE PTR [\extra_bits_table + \code]
    read_lsbf_field r9
    add \result, rbx
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
    dec rcx
    jnz .L_loop_\@
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
    mov r8, rax
    mov r9, rdi
    xor rax, rax
    lea rdi, VAR(deflate_data_start)
    lea rcx, VAR(deflate_data_end)
    sub rcx, rdi
    rep stosb
    mov rax, r8
    mov rdi, r9
    # read HLIT, HDIST and HCLEN
    ensure_enough_bits
    read_lsbf_fs_field 5
    mov BYTE PTR VAR(lit_sorted_table), bl
    read_lsbf_fs_field 5
    mov BYTE PTR VAR(dist_sorted_table), bl
    read_lsbf_fs_field 4

    # Read CLEN codes
    add rbx, 4
    xor r9, r9
    mov r11, rbx
1:
    ensure_enough_bits
    read_lsbf_fs_field 3
    movzx r10, BYTE PTR [r9 + clen_codes]
    mov BYTE PTR IN_VAR(len_table, r10), bl
    inc r9
    cmp r9, r11
    jne 1b

    compute_prefix_counts clen_prefix_table, 19
    compute_initial_prefix_codes_and_offsets clen_prefix_table, 8
    compute_prefix_table_sorted_table clen_prefix_table, clen_sorted_table, 19, 1

# Fill in lit tables
    movzx r12, BYTE PTR VAR(lit_sorted_table)
    add r12, 257
    read_encoded_len_table r12
    compute_prefix_counts lit_prefix_table, 286
    compute_initial_prefix_codes_and_offsets lit_prefix_table, 16
    compute_prefix_table_sorted_table lit_prefix_table, lit_sorted_table, 286, 2

# Fill in dist tables
    movzx r12, BYTE PTR VAR(dist_sorted_table)
    inc r12
    read_encoded_len_table r12
    compute_prefix_counts dist_prefix_table, 32
    compute_initial_prefix_codes_and_offsets dist_prefix_table, 16
    compute_prefix_table_sorted_table dist_prefix_table, dist_sorted_table, 32, 1

decoding_loop:
    ensure_enough_bits
    read_huffman_encoded_value lit_prefix_table, lit_sorted_table, 2
    cmp rbx, 0x100
    je decoding_loop_exit
    jb literal
    sub rbx, 0x100
    ensure_enough_bits
    read_extra_bits rbx, length_offsets, length_extra_bits, rcx
    ensure_enough_bits
    read_huffman_encoded_value dist_prefix_table, dist_sorted_table, 1
    ensure_enough_bits
    read_extra_bits rbx, dist_offsets, dist_extra_bits, r12
    mov r8, rsi
    mov rsi, rdi
    sub rsi, r12
    overlapping_memcpy r12
    # memcpy
    mov rsi, r8
    jmp decoding_loop
literal:
    mov BYTE PTR [rdi], bl
    inc rdi
    jmp decoding_loop

decoding_loop_exit:

.endm

.macro deflate
    xor rax, rax
    xor rdx, rdx
    mov BYTE PTR VAR(last_block_flag), 0
read_next_block:
    mov r8b, BYTE PTR VAR(last_block_flag)
    test r8b, r8b
    jnz exit
    ensure_enough_bits
    read_lsbf_fs_field 1
    mov BYTE PTR VAR(last_block_flag), bl
    read_lsbf_fs_field 2
    cmp rbx, 1
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
    test rbx, rbx
    jz read_next_block
    cmp rbx, 0x17
    jbe fixed_coded_7
    advance_msbf_value
    cmp rbx, 0xbf
    jbe fixed_literal_8
    cmp rbx, 0xc7
    jbe fixed_coded_8
    advance_msbf_value
    sub rbx, 0x190
    mov BYTE PTR [rdi], bl
    inc rdi
    jmp fixed_compression
fixed_coded_8:
    sub rbx, 0xc0
fixed_coded_7:
    read_extra_bits rbx, length_offsets, length_extra_bits, rcx
    read_msbf_fs_field 5
    read_extra_bits rbx, dist_offsets, dist_extra_bits, r9
    mov r8, rsi
    mov rsi, rdi
    sub rsi, r9
    memcpy
    mov rsi, r8
    jmp fixed_compression
fixed_literal_8:
    sub rbx, 0x30
    mov BYTE PTR [rdi], bl
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
