#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_IOCTL 16
#define SYS_MSYNC 26
#define SYS_EXIT 60
#define SYS_FTRUNCATE 77
#define SYS_MKDIR 83
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258

#define STDOUT 1
#define STDIN 0

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

//msync defines
#define MS_SYNC 4

