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
	limit.rlim_max = MAX_TIME / 1000 + 1;		// second(s)

	if (setrlimit(RLIMIT_CPU, &limit))
		MSG_ERR_EXIT(SYSTEM_ERROR, "Set Time Limit Failed");

	limit.rlim_cur = MAX_STACK_SIZE;
	limit.rlim_max = MAX_STACK_SIZE;		// byte(s)

	if (setrlimit(RLIMIT_STACK, &limit))
		MSG_ERR_EXIT(SYSTEM_ERROR, "Set Memory Limit Failed");

#if 0
	limit.rlim_cur = MAX_MEMORY * (1 << 20);
	limit.rlim_max = MAX_MEMORY * (1 << 20);	// byte(s)

	/* The maximum size of the process's virtual memory (address space) in bytes */
	if (setrlimit(RLIMIT_AS, &limit))
		MSG_ERR_EXIT(SYSTEM_ERROR, "Set Memory Limit Failed");
#endif

}

#define kill_it(pid) ptrace(PTRACE_KILL, pid, NULL, NULL)

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
		MSG_ERR_RET(1, "Accessing File: \"%s\"\n", (const char*)access_file);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int result = ACCEPTED;
	int status;
	
	struct rusage usage;
	struct user_regs_struct regs;

	if (2 != argc)
		MSG_ERR_RET(EXIT_FAILURE, "Usage: %s executable\n", argv[0]);

	const char *binary = argv[1];

	pid_t child = fork();

	/* fork a child to monitor(ptrace) its status */
	if (0 == child) {
		setResourceLimit();

		if (ptrace(PTRACE_TRACEME, 0, NULL, NULL))
			MSG_ERR_EXIT(SYSTEM_ERROR, "PTRACE_TRACEME Failed");

		if (-1 == execl(binary, "", NULL))
			MSG_ERR_EXIT(SYSTEM_ERROR, "execl() Failed");
	}

	/* assure that parent gets executed after child exits */
	if (-1 == child) {
		MSG_ERR_RET(SYSTEM_ERROR, "vfork() Failed");
	}

	for (;;) {
		if (-1 == wait4(child, &status, WSTOPPED, &usage))
			MSG_ERR_RET(SYSTEM_ERROR, "wait4() Failed");

		/* child has already exited */
		if (WIFEXITED(status)) {
			if (SYSTEM_ERROR == WEXITSTATUS(status))
				return SYSTEM_ERROR;
		}

		if (SIGTRAP != WSTOPSIG(status)) {
			kill_it(child);

			switch (WSTOPSIG(status)) {
			case SIGSEGV:
				result = RUNTIME_ERROR;
				//MSG_ERR_RET(RUNTIME_ERROR, "SIGSEGV");
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
			char cmd[1024];
			snprintf(cmd, sizeof(cmd), "grep -E "
				"'*SYS.*.[[:space:]]%llu$'"
				" /usr/include/x86_64-linux-musl/bits/syscall.h"
				" | gawk -F' ' '{print $2}' 1>&2", REG_SYS_CALL(&regs));
			system(cmd);
			MSG_ERR_RET(RUNTIME_ERROR, "Invalid Syscall");
		}

		/* watch what the child is going to open */
		if (SYS_open == REG_SYS_CALL(&regs)) {
			if (invalidAccess(child, &regs))
				MSG_ERR_RET(RUNTIME_ERROR, "Invalid Access");
		}

		/* allocate too much from data section */
		if (SYS_writev == REG_SYS_CALL(&regs))
			MSG_ERR_RET(MEMORY_LIMIT_EXCEEDED, "ENOMEM on BSS/DATA");

		/* trace next system call */
		if (-1 == ptrace(PTRACE_SYSCALL, child, NULL, NULL))
			MSG_ERR_RET(SYSTEM_ERROR, "PTRACE_SYSCALL Failed");
	}
END:
	Time = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000
		+ usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000;
	Memory = usage.ru_maxrss * (sysconf(_SC_PAGESIZE) / 1024) / 1024;

	if (MAX_MEMORY <= Memory)
		result = MEMORY_LIMIT_EXCEEDED;

	if (ACCEPTED == result)
		fprintf(stderr, "TIME: %ldMS MEM: %ldMB\n", Time, Memory);

	return result;
}
