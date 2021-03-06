/*
	Given the input and output file, respectively,
	decide whether a specified source is satiesfied.
*/

#include "common.h"

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

static void setRlimit()
{
	struct rlimit limit;

	limit.rlim_cur = MAX_TIME / 1000;
	limit.rlim_max = MAX_TIME / 1000 + 1;	// second(s)

	if (setrlimit(RLIMIT_CPU, &limit))
		EXIT_MSG("Set Time Limit Failed", SYSTEM_ERROR);

	limit.rlim_cur = MAX_MEMORY;
	limit.rlim_max = MAX_MEMORY;		// byte(s)

	if (setrlimit(RLIMIT_DATA, &limit))
		EXIT_MSG("Set Memory Limit Failed", SYSTEM_ERROR);
}

int main(int argc, char *argv[])
{
	int result = ACCEPTED;

	int i;
	int fd[2];
	int status;
	long file_tmp[10];

	pid_t child;
	struct rusage usage;
	struct user_regs_struct regs;

	if (4 != argc)
		EXIT_MSG("Usage: run executable in_file temp_file", EXIT_FAILURE);

	const char *bin = argv[1];
	const char *in = argv[2];
	const char *out = argv[3];

	child = vfork();
	if (child < 0)
		EXIT_MSG("vfork() Failed", SYSTEM_ERROR);

	/* fork a child to monitor(ptrace) its status */
	if (0 == child) {
		setRlimit();

		if ((fd[0] = open(in, O_RDONLY, 0644)) < 0 || dup2(fd[0], STDIN_FILENO) < 0)
			EXIT_MSG("open & dup2(STDIN) Failed", SYSTEM_ERROR);

		if ((fd[1] = creat(out, 0644)) < 0 || dup2(fd[1], STDOUT_FILENO) < 0)
			EXIT_MSG("open & dup2(STDOUT) Failed", SYSTEM_ERROR);

		if (ptrace(PTRACE_TRACEME, 0, NULL, NULL))
			EXIT_MSG("ptrace(PTRACE_TRACEME) Failed", SYSTEM_ERROR);

		if (-1 == execl(bin, "", NULL))
			EXIT_MSG("execl() Failed", SYSTEM_ERROR);
	}

	for (;;) {
		if (-1 == wait4(child, &status, WSTOPPED, &usage))
			EXIT_MSG("wait4() Failed", SYSTEM_ERROR);

		if (WIFEXITED(status)) {
			if (SYSTEM_ERROR == WEXITSTATUS(status))
				_exit(SYSTEM_ERROR);
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
			case SIGALRM:
			case SIGXCPU:
				result = TIME_LIMIT_EXCEEDED;
				break;
			}

		}

		if (-1 == ptrace(PTRACE_GETREGS, child, NULL, &regs))
			goto END;

		if (!isAllowedCall(REG_SYS_CALL(&regs))) {
			ptrace(PTRACE_KILL, child, NULL, NULL);
			result = RUNTIME_ERROR;
			goto END;
		} else if (SYS_open == REG_SYS_CALL(&regs)) {
			/* peek what the program is opening */
			for (i = 0; i < 10; i++) {
				file_tmp[i] = ptrace(PTRACE_PEEKDATA, child,
					REG_ARG_1(&regs) + i*sizeof(long), NULL);
				if (file_tmp[i] == 0)
					break;
			}
			if (!isValidAccess((const char*)file_tmp)) {
				ptrace(PTRACE_KILL, child, NULL, NULL);
				fprintf(stderr, "RUNTIME_ERROR %s\n", (const char*)file_tmp); 
				result = RUNTIME_ERROR;
				goto END;
			}
		}

		if (-1 == ptrace(PTRACE_SYSCALL, child, NULL, NULL))
			EXIT_MSG("PTRACE_SYSCALL Failed", SYSTEM_ERROR);

	}
END:
	return result;
}

