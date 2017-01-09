#include "toy.h"
#include <time.h>
#include <dirent.h>
#include <libgen.h>

#define RAISE_MSG(msg) \
	{ fprintf(stderr, "%s\n", msg); goto CLEAN; }

static int endWith(const char *haystack, const char *needle) {

	size_t haystackLen = strlen(haystack);
	size_t needleLen = strlen(needle);

	if (haystackLen < needleLen)
		return 0;

	char *end = (char *)haystack + (haystackLen - needleLen);
	return 0 == strcmp(end, needle);
}

static void randomString(char str[], size_t len) {
	int i;

	srand(time(NULL));
	for (i = 0; i < len-1; ++i) {
		str[i] = rand() %26 + 'a' ; 
	}
	str[len - 1] = '\0';
}

static int countTestdata(const char *filename_dir) {
	int n, cnt = 0;
	struct dirent **filename;

	n = scandir(filename_dir, &filename, NULL, alphasort);
	if (n < 0)
		EXIT_MSG("scandir() Failed", EXIT_FAILURE);

	while (n--) {
		if (endWith(filename[n]->d_name, ".in"))
			++cnt;
		free(filename[n]);
	}
	free(filename);
	return cnt;
}

int main(int argc, char *argv[])
{
	int num, total, iRetVal;
	char temp[9], cmd[200];
	char *relative_path;

	if (3 != argc)
		EXIT_MSG("Usage: main src.c filename_dir", EXIT_FAILURE);
	
	randomString(temp, sizeof temp);
	relative_path = dirname(argv[0]);
	total = countTestdata(argv[2]);

	snprintf(cmd, sizeof(cmd), "gcc -o %s -Wall -lm %s", temp, argv[1]);
	iRetVal = system(cmd);
	if (WEXITSTATUS(iRetVal))
		EXIT_MSG("COMPILE_ERROR", EXIT_FAILURE);

	for (num = 0; num < total; ++num) {
		snprintf(cmd, sizeof(cmd), "%s/run %s %s/%d.in %s.temp",
			relative_path, temp, argv[2], num, temp);
		iRetVal = system(cmd);
		if (WEXITSTATUS(iRetVal)) {
			switch (WEXITSTATUS(iRetVal)) {
				case SYSTEM_ERROR: RAISE_MSG("SYSTEM_ERROR");
				case RUNTIME_ERROR: RAISE_MSG("RUNTIME_ERROR");
				case TIME_LIMIT_EXCEEDED: RAISE_MSG("TIME_LIMIT_EXCEEDED");
				case MEMORY_LIMIT_EXCEEDED: RAISE_MSG("MEMORY_LIMIT_EXCEEDED");
			}
		}

		snprintf(cmd, sizeof(cmd), "%s/diff %s/%d.out %s.temp",
			relative_path, argv[2], num, temp);
		iRetVal = system(cmd);
		if (WEXITSTATUS(iRetVal)) {
			switch (WEXITSTATUS(iRetVal)) {
				case OUTPUT_LIMIT_EXCEEDED: RAISE_MSG("OUTPUT_LIMIT_EXCEEDED");
				case PRESENTATION_ERROR: RAISE_MSG("PRESENTATION_ERROR");
				case WRONG_ANWSER: RAISE_MSG("WRONG_ANWSER");
				case SYSTEM_ERROR: RAISE_MSG("SYSTEM_ERROR");
				case ACCEPTED: printf("ACCEPTED\n");
			}
		}
	}

CLEAN:
/*
	snprintf(cmd, sizeof(cmd), "rm %s %s.temp", temp, temp);
	iRetVal = system(cmd);
	if (WEXITSTATUS(iRetVal))
		EXIT_MSG("rm Failed", SYSTEM_ERROR);
*/
	return EXIT_SUCCESS;
}

