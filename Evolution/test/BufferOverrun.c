#include <stdio.h>

int main() {
	int a, b;
	while (2 == scanf("%d%d", &a, &b))
		printf("%d\n", a+b);

	int len = 0x4;
	int s[len];
	int *p = s;
	for (int i = 0; i <= len+4; ++i) 
		*p++ = 0;

}
