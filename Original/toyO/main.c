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
#include <error.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#if __WORDSIZE == 64
	#define REG_SYS_CALL(x) ((x)->orig_rax)
	#define REG_ARG_1(x) ((x)->rdi)
	#define REG_ARG_2(x) ((x)->rsi)
#else
	#define REG_SYS_CALL(x) ((x)->orig_eax)
	#define REG_ARG_1(x) ((x)->ebi)
	#define REG_ARG_2(x) ((x)->eci)
#endif

#define MAX_MEMORY (1024*1024*16)
#define MAX_OUTPUT (1<<25)

#define EXIT_MSG(msg, res) \
	do { fprintf(stderr, "%s\n", msg); _exit(res); } while (0)
#define RET_MSG(msg, res) \
	do { fprintf(stderr, "%s\n", msg); return(res); } while (0)
#define RAISE_MSG(msg) \
	do { printf("%s , %s\n", \
		baseName(argv[1]), msg); goto CLEAN; } while (0)

long Time, Memory;

enum {
	SYSTEM_ERROR = 1,
	COMPILE_ERROR,
	RUNTIME_ERROR,
	TIME_LIMIT_EXCEEDED,
	MEMORY_LIMIT_EXCEEDED,
	OUTPUT_LIMIT_EXCEEDED,
	PRESENTATION_ERROR,
	WRONG_ANWSER,
	ACCEPTED,
};

const int strace[] = {
	SYS_mmap,
	SYS_open,
	SYS_read,
	SYS_write,
	SYS_close,
	SYS_access,
	SYS_mprotect,
	SYS_brk,
	SYS_fstat,
	SYS_execve,
	SYS_arch_prctl,
	SYS_munmap,
	SYS_exit_group,
	-1,
};

const char *ltrace[] = {
	"/etc/ld.so.cache",
	"/lib/x86_64-linux-gnu/libc.so.6",
	"/lib/x86_64-linux-gnu/libm.so.6",
	NULL,
};

int isAllowedCall(int syscall) {
	int i;

	for (i = 0; -1 != strace[i]; ++i)
		if (syscall == strace[i])
			return 1;
	return 0;
}

int isValidAccess(const char *file) {
	int i;

	for (i = 0; ltrace[i]; ++i)
		if (0 == strcmp(file, ltrace[i]))
			return 1;
	return 0;
}

void setRlimit() {
	struct rlimit limit;

	limit.rlim_cur = 1;
	limit.rlim_max = 2;		// second(s)
	if (setrlimit(RLIMIT_CPU, &limit))
		EXIT_MSG("Set Time Limit Failed", SYSTEM_ERROR);

	limit.rlim_cur = MAX_MEMORY;
	limit.rlim_max = MAX_MEMORY;	// byte(s)
	if (setrlimit(RLIMIT_AS, &limit))
		EXIT_MSG("Set Memory Limit Failed", SYSTEM_ERROR);
}

void forkChild(const char *exec, const char *in, const char *temp) {
	int fd[2];
	setRlimit();

	if ((fd[0] = open(in, O_RDONLY, 0644)) < 0 || dup2(fd[0], STDIN_FILENO) < 0)
		EXIT_MSG("dup2(STDIN_FILENO) Failed", SYSTEM_ERROR);

	if ((fd[1] = creat(temp, 0644)) < 0 || dup2(fd[1], STDOUT_FILENO) < 0)
		EXIT_MSG("dup2(STDOUT_FILENO) Failed", SYSTEM_ERROR);

	if (ptrace(PTRACE_TRACEME, 0, NULL, NULL))
		EXIT_MSG("PTRACE_TRACEME Failed", SYSTEM_ERROR);

	if (-1 == execl(exec, "", NULL))
		EXIT_MSG("execl() Failed", SYSTEM_ERROR);
}

int run(const char *exec, const char *in, const char *temp) {
	int i;
	int status;
	int result = EXIT_SUCCESS;
	long file_test_tmp[100];
	struct rusage usage;
	struct user_regs_struct regs;
	pid_t child;

	child = vfork();
	if (child < 0) {
		RET_MSG("vfork() Failed", SYSTEM_ERROR);
	}
	if (0 == child) {
		forkChild(exec, in, temp);
	}
	for ( ; ; ) {
		if (-1 == wait4(child, &status, WSTOPPED, &usage))
			RET_MSG("wait4() Failed", SYSTEM_ERROR);

		if (WIFEXITED(status)) {
			if (SYSTEM_ERROR == WEXITSTATUS(status))
				return(SYSTEM_ERROR);
			break;
		} else if (SIGTRAP != WSTOPSIG(status)) {
			ptrace(PTRACE_KILL, child, NULL, NULL);
			switch (WSTOPSIG(status)) {
				case SIGSEGV:
				if (usage.ru_maxrss * (sysconf(_SC_PAGESIZE)) / MAX_MEMORY < 2)
					result = MEMORY_LIMIT_EXCEEDED;
				else
					result = RUNTIME_ERROR;
				break;
				case SIGALRM: case SIGXCPU:
				result = TIME_LIMIT_EXCEEDED;
				break;
			}
		}

		if (-1 == ptrace(PTRACE_GETREGS, child, NULL, &regs))
			goto END;

		if (!isAllowedCall(REG_SYS_CALL(&regs))) {
			ptrace(PTRACE_KILL, child, NULL, NULL);
			RET_MSG("Invalid Syscall", RUNTIME_ERROR);
		} else if (SYS_open == REG_SYS_CALL(&regs)) {
			for (i = 0; i < 100; i++) {
				file_test_tmp[i] = ptrace(PTRACE_PEEKDATA, child, REG_ARG_1(&regs) + i * sizeof(long), NULL);
				if (0 == file_test_tmp[i])
					break;
			}
			if (!isValidAccess((const char*)file_test_tmp)) {
				ptrace(PTRACE_KILL, child, NULL, NULL);
				RET_MSG("Invalid Access", RUNTIME_ERROR);
			}
			// printf("%s:%lld\n", (const char*)file_test_tmp, REG_ARG_2(&regs));
		}
		if (-1 == ptrace(PTRACE_SYSCALL, child, NULL, NULL))
			RET_MSG("PTRACE_SYSCALL Failed", SYSTEM_ERROR);

	}
END:
	Time = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000
		+ usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000;
	Memory = usage.ru_maxrss * (sysconf(_SC_PAGESIZE) / 1024);

	return result;
}

int diff(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		++s1;
		++s2;
	}
	if (!(*s1 || *s2))
		return ACCEPTED;
	for ( ; ; ) {
		while (isspace(*s1))
			s1++;
		while (isspace(*s2))
			s2++;
		if (!(*s1 && *s2))
			break;
		if (*s1 != *s2)
			return WRONG_ANWSER;
		++s1;
		++s2;
	}
	return (*s1 || *s2) ? WRONG_ANWSER : PRESENTATION_ERROR;
}

int check(const char *rightOutput, const char *userOutput) {
	int fd[2];
	int result;
	char *rightOut, *userOut;
	off_t rightLen, userLen;
	
	if ((fd[0] = open(rightOutput, O_RDONLY, 0644)) < 0)
		RET_MSG("open(out) Failed.", SYSTEM_ERROR);

	if ((fd[1] = open(userOutput, O_RDONLY, 0644)) < 0)
		RET_MSG("open(test_tmp) Failed.", SYSTEM_ERROR);

	rightLen = lseek(fd[0], 0, SEEK_END);
	userLen = lseek(fd[1], 0, SEEK_END);

	if (-1 == rightLen || -1 == userLen)
		RET_MSG("lseek() Failed", SYSTEM_ERROR);
	if (MAX_OUTPUT <= userLen)
		return OUTPUT_LIMIT_EXCEEDED;

	if (0 == (rightLen || userLen))
		return ACCEPTED;
	else if (0 == (rightLen && userLen))
		return WRONG_ANWSER;

	lseek(fd[0], 0, SEEK_SET);
	lseek(fd[1], 0, SEEK_SET);

	if ((rightOut = mmap(NULL, rightLen, PROT_READ, MAP_PRIVATE, fd[0], 0)) == MAP_FAILED)
		RET_MSG("mmap(rightOut) Failed", SYSTEM_ERROR);
	if ((userOut = mmap(NULL, userLen, PROT_READ, MAP_PRIVATE, fd[1], 0)) == MAP_FAILED)
		RET_MSG("mmap(userOut) Failed", SYSTEM_ERROR);

	result = diff(rightOut, userOut);

	munmap(rightOut, rightLen);
	munmap(userOut, userLen);
	close(fd[0]);
	close(fd[1]);
	return result;
}

int endWith(const char *haystack, const char *needle) {
	size_t haystackLen = strlen(haystack);
	size_t needleLen = strlen(needle);

	if (haystackLen < needleLen)
		return 0;

	char *end = (char *)haystack + (haystackLen - needleLen);
	return 0 == strcmp(end, needle);
}

void randomString(char str[], size_t len) {
	int i;

	srand(time(NULL));
	for (i = 0; i < len-1; ++i) {
		str[i] = rand()%26 + 'a'; 
	}
	str[len - 1] = '\0';
}

int countTestdata(const char *filename_dir) {
	int n, cnt = 0;
	struct dirent **filename;

	n = scandir(filename_dir, &filename, NULL, alphasort);
	if (n < 0)
		EXIT_MSG("scandir() Failed", EXIT_FAILURE);
	else
		while (n--) {
			if (endWith(filename[n]->d_name, ".in"))
				++cnt;
			free(filename[n]);
		}
	free(filename);
	return cnt;
}

char *baseName(char *name) {
	char *p = basename(name);
	size_t len = strlen(p);
	p[len - 2] = 0;
	return p;
}

int main(int argc, char *argv[], char *env[]) {
	int result = ACCEPTED;
	int num, total;
	char temp[9], cmd[200];
	char test_in[100], test_out[100], test_tmp[100];

	if (3 != argc)
		EXIT_MSG("Usage: main src.c filename_dir", EXIT_FAILURE);

	randomString(temp, sizeof temp);
	total = countTestdata(argv[2]);

	snprintf(cmd, sizeof(cmd), "clang -o %s -Wall -lm %s", temp, argv[1]);
	if (WEXITSTATUS(system(cmd)))
		EXIT_MSG("Compile Error", EXIT_FAILURE);

	for (num = 0; num < total; ++num) {
		snprintf(test_in ,sizeof test_in, "%s/%d.in", argv[2], num);
		snprintf(test_tmp, sizeof test_tmp, "%s.temp", temp);
		switch (run(temp, test_in, test_tmp)) {
			case SYSTEM_ERROR:
				RAISE_MSG("System Error");
			case RUNTIME_ERROR:
				RAISE_MSG("Runtime Error");
			case TIME_LIMIT_EXCEEDED:
				RAISE_MSG("Time Limit Exceeded");
			case MEMORY_LIMIT_EXCEEDED:
				RAISE_MSG("Memory Limit Exceeded");
		}
		
		snprintf(test_out, sizeof test_out, "%s/%d.out", argv[2], num);
		snprintf(test_tmp, sizeof test_tmp, "%s.temp", temp);
		switch (check(test_out, test_tmp)) {
			case OUTPUT_LIMIT_EXCEEDED:
				RAISE_MSG("Output Limit Exceeded");
			case PRESENTATION_ERROR:
				RAISE_MSG("Presentation Error");
			case WRONG_ANWSER:
				RAISE_MSG("Wrong Anwser");
			case SYSTEM_ERROR:
				RAISE_MSG("System Error");
			case ACCEPTED: ;
		}
	}

	if (ACCEPTED == result)
		printf("%s , %s Time: %ldMS Memory: %ldKB\n",
			baseName(argv[1]), "Accepted", Time, Memory);

CLEAN:
	snprintf(cmd, sizeof(cmd), "rm %s %s.temp", temp, temp);
	if (WEXITSTATUS(system(cmd)))
		EXIT_MSG("rm Failed", SYSTEM_ERROR);

	return EXIT_SUCCESS;
}

