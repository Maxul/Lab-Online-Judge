#include "common.h"

/* user-defined getline() */
ssize_t usr_getline(char *string, int lim, FILE *stream)
{
	ssize_t pos = -1;
	int ch;

	if ( !string || !lim || !stream ) {
		errno = EINVAL;
		return -1;
	}

	for (pos = 0; pos < lim-1 && (ch=fgetc(stream)) != EOF && ch != '\n'; ++pos)
        	string[pos] = ch;

	if (ch == '\n')
		string[pos++] = ch;

	string[pos] = '\0';
	return pos;
}

int main(int argc, char const *argv[])
{
	FILE *fp_in, *fp_out, *fp_tmp;
	char line_in[80], line_out[80], line_tmp[80];
	size_t len_in = 0, len_out = 0, len_tmp = 0;

	fp_in = fopen(argv[1], "r");
	fp_out = fopen(argv[2], "r");
	fp_tmp = fopen(argv[3], "r");

	if (!fp_in || !fp_out || !fp_tmp)
		return EXIT_FAILURE;

	while (-1 != usr_getline(line_tmp, sizeof(line_tmp), fp_tmp)
		&& -1 != usr_getline(line_out, sizeof(line_out), fp_out)) {
		if (-1 != usr_getline(line_in, sizeof(line_in), fp_in)
			&& 0 != strcmp(line_out, line_tmp)) {
				fprintf(stderr, "Input:%sOutput:%sExpected:%s",
					line_in, line_tmp, line_out);
				break;
		}
	}

	if (fclose(fp_in) || fclose(fp_out) || fclose(fp_tmp))
		EXIT_MSG("fclose() Failed", EXIT_FAILURE);
	return 0;
}
