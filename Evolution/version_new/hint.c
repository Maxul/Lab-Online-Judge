/*
	2016/6/8
	Original Author: Maxul Lee
	<lmy2010lmy@gmail.com>
*/

#include "common.h"

/*
	user-defined getline()
*/
static int getline2(char *s, int lim, FILE *fp)
{
	int c, i = 0;

	while (i < lim-1 && EOF != (c = fgetc(fp)) && '\n' != c)
		s[i++] = c;

	if ('\n' == c)
		s[i++] = '\0';

	s[lim - 1] = '\0';
	return i;
}

int main(int argc, char const *argv[])
{
	FILE *fp_in, *fp_out, *fp_tmp;
	char line_in[MAX_LINE_LEN], line_out[MAX_LINE_LEN], line_tmp[MAX_LINE_LEN];

	if (4 != argc)
		MSG_ERR_RET(EXIT_FAILURE, "Usage: %s in_file out_file temp_file\n", argv[0]);

	fp_in = fopen(argv[1], "r");
	fp_out = fopen(argv[2], "r");
	fp_tmp = fopen(argv[3], "r");

	if (!fp_in || !fp_out || !fp_tmp)
		MSG_ERR_RET(SYSTEM_ERROR, "fopen() Failed");

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
		MSG_ERR_RET(SYSTEM_ERROR, "fclose() Failed");

	return EXIT_SUCCESS;
}

