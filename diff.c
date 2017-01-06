#include "common.h"

int getline2(char *s, int lim, FILE *fp) {
    int c, i = -1;
    for (i = 0; i < lim-1 && (c=fgetc(fp))!=EOF && c!='\n'; ++i)
        s[i] = c;
    if (c == '\n')
        s[i++] = c;
    s[i] = '\0';
    return i;
}

int main(int argc, char const *argv[])
{
	FILE *in, *out, *tmp;
	char line_in[80], line_out[80], line_tmp[80];
	size_t len_in = 0, len_out = 0, len_tmp = 0;

	in = fopen(argv[1], "r");
	out = fopen(argv[2], "r");
	tmp = fopen(argv[3], "r");

	if (!in || !out || !tmp)
		exit(EXIT_FAILURE);

	while (-1 != getline2(line_tmp, 80, tmp)
		&& -1 != getline2(line_out, 80, out)) {
		if (-1 != getline2(line_in, 80, in) && 0 != strcmp(line_out, line_tmp)) {
				fprintf(stderr, "Input:%sOutput:%sExpected:%s",
					line_in, line_tmp, line_out);
				break;
		}
	}
	if (fclose(in) || fclose(out) || fclose(tmp))
		EXIT_MSG("fclose() Failed", EXIT_FAILURE);
	return 0;
}
