#define _GNU_SOURCE
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/reg.h>

#include <libgen.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

/* check target platform */
#if __WORDSIZE == 64
	#define REG_SYS_CALL(x) ((x)->orig_rax)
	#define REG_ARG_1(x) ((x)->rdi)
#else
	#define REG_SYS_CALL(x) ((x)->orig_eax)
	#define REG_ARG_1(x) ((x)->ebi)
#endif

/* total size of memory that one program can possess */
#define MAX_MEMORY (1 << 24)

/* total length of time that one program can possess */
#define MAX_TIME (1500)

/* restrict maximum lines of printed output */
#define MAX_OUTPUT (1 << 22)
#define MAX_LINE_LEN 80

/* in case of error occurence */
#define MSG_ERR_EXIT(msg, ret) \
	do { fprintf(stderr, "%s\n", msg); _exit(ret); } while (0)

#define MSG_ERR_RET(msg, ret) \
	do { fprintf(stderr, "%s\n", msg); return(ret); } while (0)


/* all kinds of judged results */
enum {
	ACCEPTED = 1,

	COMPILE_ERROR,

	MEMORY_LIMIT_EXCEEDED,
	OUTPUT_LIMIT_EXCEEDED,

	RUNTIME_ERROR,
	SYSTEM_ERROR,

	TIME_LIMIT_EXCEEDED,

	PRESENTATION_ERROR,
	WRONG_ANWSER,
};

/* allowed system call list */
const int strace[] = {

	SYS_open,
	SYS_read,
	SYS_write,
	SYS_lseek,
	SYS_close,
	SYS_access,

	SYS_mprotect,
	SYS_brk,
	SYS_uname,
	SYS_fstat,
	SYS_execve,
	SYS_readlink,
	SYS_arch_prctl,
	SYS_mmap,
	SYS_munmap,

	SYS_exit_group,
	-1, /* the end flag */
};

/* allowed library mapping list */
const char *ltrace[] = {
	"/etc/ld.so.cache",
	/* for C */
	"/lib/x86_64-linux-gnu/libc.so.6", /* in this case it's on x86-64 */
	"/lib/x86_64-linux-gnu/libm.so.6",
	/* for C++ */
	"/lib/x86_64-linux-gnu/libgcc_s.so.1",
	"/usr/lib/x86_64-linux-gnu/libstdc++.so.6",
	NULL, /* the end flag */
};

