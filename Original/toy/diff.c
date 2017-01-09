#include "toy.h"

/*
	when the tested source file has been compiled and
	successfully produced the output file, this function
	will perform the answer checking exercise.
*/
static int diff(const char *s1, const char *s2) {

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

int main(int argc, char const *argv[])
{
	int result;
	char *right_out, *user_out;
	off_t right_len, user_len;
	int right_fd, user_fd;

	if (3 != argc)
		EXIT_MSG("Usage: diff out tmp", EXIT_FAILURE);

	if ((right_fd = open(argv[1], O_RDONLY, 0644)) < 0)
		EXIT_MSG("open(out) Failed.", SYSTEM_ERROR);
	if ((user_fd = open(argv[2], O_RDONLY, 0644)) < 0)
		EXIT_MSG("open(tmp) Failed.", SYSTEM_ERROR);

	right_len = lseek(right_fd, 0, SEEK_END);
	user_len = lseek(user_fd, 0, SEEK_END);

	if (-1 == right_len || -1 == user_len)
		EXIT_MSG("lseek() Failed", SYSTEM_ERROR);
	if (MAX_OUTPUT <= user_len)
		return OUTPUT_LIMIT_EXCEEDED;

	if (0 == (right_len || user_len))
		return ACCEPTED;
	else if (0 == (right_len && user_len))
		return WRONG_ANWSER;

	lseek(right_fd, 0, SEEK_SET);
	lseek(user_fd, 0, SEEK_SET);

	if ((right_out = mmap(NULL, right_len, PROT_READ, MAP_PRIVATE, right_fd, 0)) == MAP_FAILED)
		EXIT_MSG("mmap(right_out) Failed", SYSTEM_ERROR);
	if ((user_out = mmap(NULL, user_len, PROT_READ, MAP_PRIVATE, user_fd, 0)) == MAP_FAILED)
		EXIT_MSG("mmap(user_out) Failed", SYSTEM_ERROR);
	
	result = diff(right_out, user_out);

	munmap(right_out, right_len);
	munmap(user_out, user_len);

	close(right_fd);
	close(user_fd);

	return result;
}

