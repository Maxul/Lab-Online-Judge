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


/*
	This main.c only does one job fairly well:
	given an executable candidate, check whether
	it satisfies the following rule - i.e. for
	each target input, the candidate is capable
	of delivering the output as expect. If not,
	report the reason.
*/

#include "common.h"

#define MSG_ERR_RET(msg, res) \
	do { fprintf(stderr, "%s\n", msg); return(res); } while (0)

#define MSG_JUDGE_QUIT(msg) \
	do { printf("%s\n", msg); goto FINAL; } while (0)

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
static void setResourceLimit() {
	struct rlimit limit;

	limit.rlim_cur = MAX_TIME / 1000;
	limit.rlim_max = MAX_TIME / 1000 + 1;	// second(s)

	if (setrlimit(RLIMIT_CPU, &limit))
		EXIT_MSG("Set Time Limit Failed", SYSTEM_ERROR);

	limit.rlim_cur = MAX_MEMORY;
	limit.rlim_max = MAX_MEMORY;		// byte(s)

	if (setrlimit(RLIMIT_AS, &limit))
		EXIT_MSG("Set Memory Limit Failed", SYSTEM_ERROR);
}

#define kill_it(pid) ptrace(PTRACE_KILL, pid, NULL, NULL);

static int invalidAccess(pid_t pid, struct user_regs_struct *registers) {
	int i;
	long access_file[16];

	/* peek which file the process is about to open */
	for (i = 0; i < 16; i++) {
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

/*
	Given the input and output file, respectively,
	decide whether a specified source is satiesfied.
*/
static int run(const char *bin, const char *in, const char *out) {
	int result = EXIT_SUCCESS;
	int status;
	
	struct rusage usage;
	struct user_regs_struct regs;
	
	pid_t child = vfork();

	/* assure that parent gets executed after child exits */
	if (child < 0) {
		MSG_ERR_RET("vfork() Failed", SYSTEM_ERROR);
	}
	/* fork a child to monitor(ptrace) its status */
	if (0 == child) {
		int fd[2];

		setResourceLimit();

		/* dup2 guarantees the atomic operation */
		
		if ((fd[0] = open(in, O_RDONLY, 0644)) < 0 || dup2(fd[0], STDIN_FILENO) < 0)
			EXIT_MSG("open() & dup2(STDIN_FILENO) Failed", SYSTEM_ERROR);

		if ((fd[1] = creat(out, 0644)) < 0 || dup2(fd[1], STDOUT_FILENO) < 0)
			EXIT_MSG("creat() & dup2(STDOUT_FILENO) Failed", SYSTEM_ERROR);

		if (ptrace(PTRACE_TRACEME, 0, NULL, NULL))
			EXIT_MSG("PTRACE_TRACEME Failed", SYSTEM_ERROR);

		if (-1 == execl(bin, "", NULL)) {
			printf("%s", bin);
			EXIT_MSG("execl() Failed", SYSTEM_ERROR);
		}
	}

	for (;;) {
		if (-1 == wait4(child, &status, WSTOPPED, &usage))
			MSG_ERR_RET("wait4() Failed", SYSTEM_ERROR);

		/* child has already exited */
		if (WIFEXITED(status)) {

			if (SYSTEM_ERROR == WEXITSTATUS(status))
				return SYSTEM_ERROR;

		} else if (SIGTRAP != WSTOPSIG(status)) {
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

	return result;
}

static int diff(const char *s1, const char *s2) {
	/* test if the same */
	while (*s1 && *s1 == *s2) {
		++s1;
		++s2;
	}

	/* both encounter NUL */
	if (!(*s1 || *s2))
		return ACCEPTED;

	for (;;) {
		/* skip invisible characters */
		while (isspace(*s1))
			s1++;
		while (isspace(*s2))
			s2++;
		/* either is NUL, no need to compare more */
		if (!(*s1 && *s2))
			break;
		/* neither is NUL */
		if (*s1 != *s2)
			return WRONG_ANWSER;
		/* keep running */
		++s1;
		++s2;
	}
	return (*s1 || *s2) ? WRONG_ANWSER : PRESENTATION_ERROR;
}

/*
	When the tested source file has been compiled and
	successfully produced the output file, this function
	will perform the answer checking exercise.
*/

static int check(const char *out, const char *tmp) {
	int fd[2];
	int result = ACCEPTED;
	char *out_mem, *tmp_mem;
	off_t out_len, tmp_len;
	
	if ((fd[0] = open(out, O_RDONLY, 0644)) < 0)
		MSG_ERR_RET("open(out) Failed", SYSTEM_ERROR);

	if ((fd[1] = open(tmp, O_RDONLY, 0644)) < 0)
		MSG_ERR_RET("open(tmp) Failed", SYSTEM_ERROR);

	/* collect length infomation */
	out_len = lseek(fd[0], 0, SEEK_END);
	tmp_len = lseek(fd[1], 0, SEEK_END);

	if (-1 == out_len || -1 == tmp_len)
		MSG_ERR_RET("lseek() Failed", SYSTEM_ERROR);

	if (MAX_OUTPUT <= tmp_len)
		return OUTPUT_LIMIT_EXCEEDED;

	/* compare them according to their size */

	/* both are empty */
	if (0 == (out_len || tmp_len))
		return ACCEPTED;
	/* either is empty */
	if (0 == (out_len && tmp_len))
		return WRONG_ANWSER;

	/* rewind */
	lseek(fd[0], 0, SEEK_SET);
	lseek(fd[1], 0, SEEK_SET);

	if (-1 == out_len || -1 == tmp_len)
		MSG_ERR_RET("lseek() or rewind Failed", SYSTEM_ERROR);

	/* map files to memory for efficiency */
	if ((out_mem = mmap(NULL, out_len, PROT_READ, MAP_PRIVATE, fd[0], 0)) == MAP_FAILED)
		MSG_ERR_RET("mmap(out_mem) Failed", SYSTEM_ERROR);
	if ((tmp_mem = mmap(NULL, tmp_len, PROT_READ, MAP_PRIVATE, fd[1], 0)) == MAP_FAILED)
		MSG_ERR_RET("mmap(tmp_mem) Failed", SYSTEM_ERROR);

	result = diff(out_mem, tmp_mem);

	/* clean */
	if (-1 == munmap(out_mem, out_len) || -1 == munmap(tmp_mem, tmp_len))
		MSG_ERR_RET("munmap() Failed", SYSTEM_ERROR);	
	/* in case of running out of available file descriptors */
	if (-1 == close(fd[0]) || -1 == close(fd[1]))
		MSG_ERR_RET("close() Failed", SYSTEM_ERROR);	
	return result;
}

/*
	Test if haystack ends with the needle
*/
static int endWith(const char *haystack, const char *needle) {
	size_t haystackLen = strlen(haystack);
	size_t needleLen = strlen(needle);

	if (haystackLen < needleLen)
		return 0;

	char *end = (char *)haystack + (haystackLen - needleLen);
	return 0 == strcmp(end, needle);
}

/*
	Generate a temporary file name
*/
static void randomString(char str[], const size_t len) {
	int i;

	srand(time(NULL));
	for (i = 0; i < len-1; ++i) {
		str[i] = rand()%26 + 'a'; 
	}
	str[len - 1] = '\0';
}

/*
	Count how many .in files there are in the folder
*/
static int countFiles(const char *directory, const char *suffix) {
	int n, cnt = 0;
	struct dirent **filename;

	n = scandir(directory, &filename, NULL, alphasort);
	if (n < 0)
		EXIT_MSG("scandir() Failed", EXIT_FAILURE);
	else
		while (n--) {
			if (endWith(filename[n]->d_name, suffix))
				++cnt;
			free(filename[n]);
		}
	free(filename);
	return cnt;
}

#define countTestdata(dir) countFiles(dir, ".in")

/*
	user-defined getline()
*/
static int getline2(char *s, int lim, FILE *fp) {
	int c, i = 0;

	while (i < lim-1 && EOF != (c = fgetc(fp)) && '\n' != c)

		s[i++] = c;
	if ('\n' == c)
		s[i++] = '\0';

	s[lim - 1] = '\0';
	return i;
}

static void compare(const char *test_in, const char *test_out, const char *test_tmp) {
	FILE *fp_in, *fp_out, *fp_tmp;
	char line_in[MAX_LINE_LEN], line_out[MAX_LINE_LEN], line_tmp[MAX_LINE_LEN];

	fp_in = fopen(test_in, "rm");
	fp_out = fopen(test_out, "rm");
	fp_tmp = fopen(test_tmp, "rm");

	if (!fp_in || !fp_out || !fp_tmp)
		EXIT_MSG("fclose() Failed", EXIT_FAILURE);

	while (getline2(line_tmp, MAX_LINE_LEN, fp_tmp) && getline2(line_out, MAX_LINE_LEN, fp_out)) {
		if (getline2(line_in, MAX_LINE_LEN, fp_in) && 0 != strcmp(line_out, line_tmp)) {
			fprintf(stderr, "Input:^%s$\n"
					"Output:^%s$\n"
					"Expected:^%s$\n",
				line_in, line_tmp, line_out);
			break;
		}
	}

	if (fclose(fp_in) || fclose(fp_out) || fclose(fp_tmp))
		EXIT_MSG("fclose() Failed", EXIT_FAILURE);
}

int main(int argc, char *argv[], char *env[]) {
	int result = ACCEPTED;

	int num, total;
	char test_temp[9];

	typedef char char32[32];
	char32 test_in, test_out;

	if (3 != argc)
		EXIT_MSG("Usage: JUDGE exec_file problem_folder", EXIT_FAILURE);

	randomString(test_temp, sizeof test_temp);

	total = countTestdata(argv[2]);

	for (num = 0; num < total; ++num) {
		snprintf(test_in ,sizeof test_in, "%s/%d.in", argv[2], num);
		switch (run(argv[1], test_in, test_temp)) {
		case SYSTEM_ERROR:
			MSG_JUDGE_QUIT("System Error");
		case RUNTIME_ERROR:
			MSG_JUDGE_QUIT("Runtime Error");
		case TIME_LIMIT_EXCEEDED:
			MSG_JUDGE_QUIT("Time Limit Exceeded");
		case MEMORY_LIMIT_EXCEEDED:
			MSG_JUDGE_QUIT("Memory Limit Exceeded");
		}

		snprintf(test_out, sizeof test_out, "%s/%d.out", argv[2], num);
		switch (check(test_out, test_temp)) {
		case OUTPUT_LIMIT_EXCEEDED:
			MSG_JUDGE_QUIT("Output Limit Exceeded");
		case PRESENTATION_ERROR:
			MSG_JUDGE_QUIT("Presentation Error");
		case WRONG_ANWSER:
			compare(test_in, test_out, test_temp);
			MSG_JUDGE_QUIT("Wrong Anwser");
		case SYSTEM_ERROR:
			MSG_JUDGE_QUIT("System Error");
		case ACCEPTED: ;
		}

	}

	if (ACCEPTED == result)
		printf("Accepted TIME: %ldMS MEM: %ldKB\n", Time, Memory);

FINAL:
	/* bye for now */
	if (-1 == unlink(test_temp))
		MSG_ERR_RET("unlink Failed", SYSTEM_ERROR);

	return EXIT_SUCCESS;
}
