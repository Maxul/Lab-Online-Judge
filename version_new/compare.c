/*
	2016/6/8
	Original Author: Maxul Lee
	<lmy2010lmy@gmail.com>
*/

#include "common.h"

static int compare(const char *s1, const char *s2)
{
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

int main(int argc, char *argv[], char *env[])
{
	int result = ACCEPTED;

	int fd[2];

	char *out_mem, *tmp_mem;
	off_t out_len, tmp_len;

	const char *out = argv[1];
	const char *tmp = argv[2];
	
	if (3 != argc) {
		fprintf(stderr, "Usage: %s out_file temp_file\n", argv[0]);
		return EXIT_FAILURE;
	}

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

	result = compare(out_mem, tmp_mem);

	/* clean */
	if (-1 == munmap(out_mem, out_len) || -1 == munmap(tmp_mem, tmp_len))
		MSG_ERR_RET("munmap() Failed", SYSTEM_ERROR);	
	/* in case of running out of available file descriptors */
	if (-1 == close(fd[0]) || -1 == close(fd[1]))
		MSG_ERR_RET("close() Failed", SYSTEM_ERROR);	
	return result;
}

