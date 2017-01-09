/*
	2016/6/8
	Original Author: Maxul Lee
	<lmy2010lmy@gmail.com>

	Let's say hello and good-bye to the world!
	In this universe of beauty and grace, set
	blank barrier to your forward path.

	To be careful, make sure every system API
	in this file has checked its return value.
*/

#include "common.h"

long Time;
long Memory;

/*
	Check whether the value in REG_SYS_CALL(x)
	is amongst the allowed system call list
*/
static int isAllowedCall(int syscall)
{
	int i;
	
	for (i = 0; -1 != strace[i]; ++i)
		if (syscall == strace[i])
			return 1;
	return 0;
}

/*
	Check whether the value in REG_ARG_1(x)
	is amongst the legal library mapping list
*/
static int isValidAccess(const char *file)
{
	int i;

	for (i = 0; ltrace[i]; ++i)
		if (0 == strcmp(file, ltrace[i]))
			return 1;
	return 0;
}

/*
	a wrap-up function for setting up resource limit
*/
static void setResourceLimit()
{
	struct rlimit limit;

	limit.rlim_cur = MAX_TIME / 1000;
	limit.rlim_max = MAX_TIME / 1000 + 1;	// second(s)

	if (setrlimit(RLIMIT_CPU, &limit))
		MSG_ERR_EXIT("Set Time Limit Failed", SYSTEM_ERROR);

	limit.rlim_cur = MAX_MEMORY;
	limit.rlim_max = MAX_MEMORY;		// byte(s)

	if (setrlimit(RLIMIT_AS, &limit))
		MSG_ERR_EXIT("Set Memory Limit Failed", SYSTEM_ERROR);
}

static void forkChild(const char *bin, const char *in, const char *out)
{
	int fd[2];

	setResourceLimit();

	/* dup2 guarantees the atomic operation */

	if ((fd[0] = open(in, O_RDONLY, 0644)) < 0 || dup2(fd[0], STDIN_FILENO) < 0)
		MSG_ERR_EXIT("dup2(STDIN_FILENO) Failed", SYSTEM_ERROR);

	if ((fd[1] = creat(out, 0644)) < 0 || dup2(fd[1], STDOUT_FILENO) < 0)
		MSG_ERR_EXIT("dup2(STDOUT_FILENO) Failed", SYSTEM_ERROR);

	if (ptrace(PTRACE_TRACEME, 0, NULL, NULL))
		MSG_ERR_EXIT("PTRACE_TRACEME Failed", SYSTEM_ERROR);

	if (-1 == execl(bin, "", NULL))
		MSG_ERR_EXIT("execl() Failed", SYSTEM_ERROR);
}

#define kill_it(pid) ptrace(PTRACE_KILL, pid, NULL, NULL);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int invalidAccess(pid_t pid, struct user_regs_struct *registers)
{
	int i;
	long access_file[16];

	/* peek which file the process is about to open */
	for (i = 0; i < ARRAY_SIZE(access_file); i++) {
		access_file[i] = ptrace(PTRACE_PEEKDATA,
			pid, REG_ARG_1(registers) + i * sizeof(long), NULL);
		if (0 == access_file[i])
			break;
	}
	if (!isValidAccess((const char*)access_file)) {
		kill_it(pid);
		fprintf(stderr, "Invalid access: %s\n", (const char*)access_file);
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int result = ACCEPTED;
	int status;
	
	struct rusage usage;
	struct user_regs_struct regs;

	if (4 != argc) {
		fprintf(stderr, "Usage: %s executable in_file temp_file\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *bin = argv[1];
	const char *in = argv[2];
	const char *out = argv[3];

	pid_t child = vfork();

	/* assure that parent gets executed after child exits */
	if (child < 0) {
		MSG_ERR_RET("vfork() Failed", SYSTEM_ERROR);
	}
	/* fork a child to monitor(ptrace) its status */
	if (0 == child) {
		forkChild(bin, in, out);
	}

	for (;;) {
		if (-1 == wait4(child, &status, WSTOPPED, &usage))
			MSG_ERR_RET("wait4() Failed", SYSTEM_ERROR);

		/* child has already exited */
		if (WIFEXITED(status)) {
			if (SYSTEM_ERROR == WEXITSTATUS(status))
				return SYSTEM_ERROR;
		}

		if (SIGTRAP != WSTOPSIG(status)) {
			kill_it(child);

			switch (WSTOPSIG(status)) {
			case SIGSEGV:
				if (usage.ru_maxrss * (sysconf(_SC_PAGESIZE)) / MAX_MEMORY < 2)
					result = MEMORY_LIMIT_EXCEEDED;
				else
					result = RUNTIME_ERROR;
				break;
			case SIGALRM:
			case SIGKILL:
			case SIGXCPU:
				result = TIME_LIMIT_EXCEEDED;
				break;
			}
		}

		/* unable to peek register info */
		if (-1 == ptrace(PTRACE_GETREGS, child, NULL, &regs))
			goto END;

		if (!isAllowedCall(REG_SYS_CALL(&regs))) {
			kill_it(child);
			fprintf(stderr, "%llu\n", REG_SYS_CALL(&regs));
			MSG_ERR_RET("Invalid Syscall", RUNTIME_ERROR);
		}

		/* watch what the child is going to open */
		if (SYS_open == REG_SYS_CALL(&regs)) {
			if (invalidAccess(child, &regs))
				MSG_ERR_RET("Invalid Access", RUNTIME_ERROR);
		}

		/* trace next system call */
		if (-1 == ptrace(PTRACE_SYSCALL, child, NULL, NULL))
			MSG_ERR_RET("PTRACE_SYSCALL Failed", SYSTEM_ERROR);
	}
END:
	Time = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000
		+ usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000;
	Memory = usage.ru_maxrss * (sysconf(_SC_PAGESIZE) / 1024);

	if (ACCEPTED == result)
		printf("TIME: %ldMS MEM: %ldKB\n", Time, Memory);

	return result;
}
