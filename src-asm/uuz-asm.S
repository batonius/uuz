.intel_syntax noprefix
.arch znver2

#include "macros.inc"
#include "linux.inc"
#include "zip.inc"
#include "deflate.inc"

#define FILE_NAME_BUFFER_SIZE 1024

.text
.global _start
.type _start, %function
_start:
begin_frame
    # IO vars
    local_var zip_file_fd, 8
    local_var zip_file_size, 8
    local_var zip_file_buffer, 8
    local_var file_name_buffer, FILE_NAME_BUFFER_SIZE
    local_var out_fd, 8
    local_var out_file_buffer, 8
    local_var out_file_buffer_length, 8
    local_var tsc, 8
    # Deflate vars
    local_var last_block_flag, 8
    local_var deflate_data_end, 0
    local_var len_table, 1*288
    local_var clen_sorted_table, 1*24
    local_var clen_prefix_table, 2*2*16
    local_var lit_sorted_table, 2*288
    local_var lit_prefix_table, 2*2*16
    local_var dist_sorted_table, 1*32
    local_var dist_prefix_table, 2*2*16
    local_var deflate_data_start, 0
    local_var padding, 1024
end_frame

    mov rax, [rbp]
    cmp rax, 1
    jbe in_file_error
    sys_call_or in_file_error, SYS_OPENAT, AT_FDCWD, [rbp+16], O_RDONLY
    mov VAR(zip_file_fd), rax
    sys_call_or in_file_error, SYS_LSEEK, rax, 0, SEEK_END
    mov VAR(zip_file_size), rax
    add rax, 1024
    sys_call_or in_file_error, SYS_MMAP, 0, rax, \
        PROT_READ, MAP_PRIVATE | MAP_POPULATE, VAR(zip_file_fd), 0
    mov r12, rax
    mov VAR(zip_file_buffer), rax
    sys_call_or in_file_error, SYS_MADVISE, rax, VAR(zip_file_size), MADV_SEQUENTIAL
    mov ebx, DWORD PTR [r12 + LFH_UNCOMPRESSED_SIZE]
    mov VAR(out_file_buffer_length), rbx
#ifdef DRYRUN
    add rbx, 1024
    sys_call_or out_file_error, SYS_MMAP, 0, rbx, \
        PROT_WRITE, MAP_SHARED | MAP_POPULATE | MAP_ANONYMOUS, -1, 0
#else
    movzx rcx, WORD PTR [r12 + LFH_FILE_NAME_LENGHT]
    cmp rcx, FILE_NAME_BUFFER_SIZE
    jge in_file_error
    lea rsi, [r12 + LFH_FILE_NAME]
    lea rdi, VAR(file_name_buffer)
    rep movsb
    mov BYTE PTR [rdi], 0
    lea rsi, VAR(file_name_buffer)
    sys_call_or out_file_error, SYS_OPENAT, AT_FDCWD, rsi, \
        O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR
    mov VAR(out_fd), rax
    sys_call_or out_file_error, SYS_FTRUNCATE, rax, rbx
    add rbx, 1024
    sys_call_or out_file_error, SYS_MMAP, 0, rbx, \
        PROT_WRITE, MAP_SHARED | MAP_POPULATE, VAR(out_fd), 0
#endif
    mov VAR(out_file_buffer), rax
    sys_call_or in_file_error, SYS_MADVISE, rax, VAR(out_file_buffer_length), MADV_SEQUENTIAL
    mov rdi, VAR(out_file_buffer)
    movzx rsi, WORD PTR [r12 + LFH_FILE_NAME_LENGHT]
    add si, WORD PTR [r12 + LFH_EXTRA_LENGTH]
    add rsi, LFH_HEADER_SIZE
    add rsi, r12
    movzx rax, WORD PTR [r12 + LFH_COMPRESSION_METHOD]
    cmp rax, COMPRESSION_METHOD_DEFLATE
    je deflate
    mov rcx, VAR(out_file_buffer_length)
    memcpy
    jmp finish
deflate:
#ifdef DEBUG
    rdtsc
    shl rdx, 32
    or rdx, rax
    mov VAR(tsc), rdx
#endif
    deflate
#ifdef DEBUG
    rdtsc
    shl rdx, 32
    or rdx, rax
    sub rdx, VAR(tsc)
    lea rdi, IN_VAR(file_name_buffer, FILE_NAME_BUFFER_SIZE - 1)
    call eprint_rdx
#endif
finish:
    sys_call SYS_CLOSE, VAR(out_fd)
    sys_call SYS_CLOSE, VAR(zip_file_fd)
    sys_call SYS_EXIT, 0
    ret

out_file_error:
    mov rbx, rax
    neg rbx
    sys_call SYS_WRITE, STDOUT, \
        "offset no_file_msg", "offset no_file_msg_len"
    sys_call SYS_EXIT, rbx
    ret

in_file_error:
    mov rbx, rax
    neg rbx
    sys_call SYS_WRITE, STDOUT, \
        "offset out_file_error_msg", "offset out_file_error_msg_len"
    sys_call SYS_EXIT, rbx
    ret
.size _start, (. - _start)

#ifdef DEBUG
eprint_rdx:
    shr rdx, 16
    mov rsi, rdi
    mov BYTE PTR [rdi], 0xa
    dec rdi
    mov rcx, 16
1:
    mov al, dl
    and al, 0xf
    cmp al, 0xa
    jge 2f
    add al, 0x30
    jmp 3f
2:
    add al, 87
3:
    mov BYTE PTR [rdi], al
    dec rdi
    shr rdx, 4
    dec rcx
    jnz 1b
    sub rsi, rdi
    inc rdi
    sys_call SYS_WRITE, STDERR, rdi, rsi
    ret
#endif

str_lit no_file_msg, "Can't open file.\n"
str_lit out_file_error_msg, "Can't create output file.\n"
deflate_tables
