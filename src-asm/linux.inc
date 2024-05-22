#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_IOCTL 16
#define SYS_MSYNC 26
#define SYS_MADVISE 28
#define SYS_EXIT 60
#define SYS_FTRUNCATE 77
#define SYS_MKDIR 83
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define SYSCALL_FAIL -4096

//openat defines
#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000
#define S_IRUSR 0400
#define S_IWUSR 0200

//lseek defines
#define SEEK_END 2

//mmap defines
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 2
#define MAP_SHARED 1
#define MAP_ANONYMOUS 0x20
#define MAP_POPULATE 0x8000
#define MAP_NONBLOCK 0x10000


//msync defines
#define MS_SYNC 4

//madvise define
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3

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

.macro sys_call_or error_label, idx=rax, a=rdi, b=rsi, c=rdx, d=r10, e=r8, f=r9
    sys_call \idx, \a, \b, \c, \d, \e, \f
    cmp rax, SYSCALL_FAIL
    ja \error_label
.endm
