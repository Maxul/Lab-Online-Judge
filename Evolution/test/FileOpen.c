#include <stdio.h>

int main() {
	int a, b;
	while (2 == scanf("%d%d", &a, &b))
		printf("%d\n", a+b);
	FILE *fp = fopen("./main.c", "r");
	fclose(fp);
}
