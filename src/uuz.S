.intel_syntax noprefix
.arch znver2

#include "macros.inc"
#include "linux.inc"
#include "zip.inc"
#include "deflate.inc"

#define FILE_NAME_BUFFER_SIZE 1024

.text
.global _start
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
    # Deflate vars
    local_var last_block_flag, 1
    local_var deflate_data_end, 0
    local_var clen_len_table, 1*19
    local_var clen_sorted_table, 1*19
    local_var clen_prefix_table, 4*8
    local_var lit_len_table, 1*286
    local_var lit_sorted_table, 2*286
    local_var lit_prefix_table, 4*16
    local_var dist_len_table, 1*32
    local_var dist_sorted_table, 1*32
    local_var dist_prefix_table, 4*16
    local_var deflate_data_start, 0
end_frame

    # > Open and mmap file
    # * [rbp] is `argc`, [rbp + 8 + 8 * n] is `argv[n]`
    mov rax, [rbp]
    cmp rax, 1
    jbe in_file_error
    sys_call_or in_file_error, SYS_OPENAT, AT_FDCWD, [rbp+16], O_RDONLY
    mov VAR(zip_file_fd), rax
    sys_call_or in_file_error, SYS_LSEEK, rax, 0, SEEK_END
    mov VAR(zip_file_size), rax
    add rax, 1024
    sys_call_or in_file_error, SYS_MMAP, 0, rax, \
        PROT_READ, MAP_PRIVATE, VAR(zip_file_fd), 0
    mov r12, rax
    mov VAR(zip_file_buffer), rax
    sys_call_or in_file_error, SYS_MADVISE, rax, VAR(zip_file_size), MADV_SEQUENTIAL
    # + r12 is the start of the current file header
    # > File processing loop
    # >> Open and mmap file
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
    mov ebx, DWORD PTR [r12 + LFH_UNCOMPRESSED_SIZE]
    # ! rbx is the uncompressed file size left
    mov VAR(out_file_buffer_length), rbx
    sys_call_or out_file_error, SYS_FTRUNCATE, rax, rbx
    add rbx, 1024
    sys_call_or out_file_error, SYS_MMAP, 0, rbx, \
        PROT_WRITE, MAP_SHARED, VAR(out_fd), 0
    mov VAR(out_file_buffer), rax
    sys_call_or in_file_error, SYS_MADVISE, rax, VAR(out_file_buffer_length), MADV_SEQUENTIAL
    mov rdi, VAR(out_file_buffer)
    # ! rdi is the uncompressed buffer position
    movzx rsi, WORD PTR [r12 + LFH_FILE_NAME_LENGHT]
    add si, WORD PTR [r12 + LFH_EXTRA_LENGTH]
    add rsi, LFH_HEADER_SIZE
    add rsi, r12
    # ! rsi is the compressed buffer position
    # - r12
    # >> do deflate
    deflate
    # sys_call_or out_file_error, SYS_MUNMAP, VAR(out_file_buffer), VAR(out_file_buffer_len)
    sys_call SYS_CLOSE, VAR(out_fd)
    # > File processing loop end
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

str_lit no_file_msg, "Can't open file.\n"
str_lit out_file_error_msg, "Can't create output file.\n"
deflate_tables
