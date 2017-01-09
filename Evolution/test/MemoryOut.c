#include <stdio.h>
#include <string.h>
int stack[1024*1024*4];
int main() {
	memset(stack, 0xf, sizeof stack);
//	int stackx[1024*4];
//	int *p = malloc(1024*1024*10); memset(p, 0xf, 1000);
	int a, b;
	while (2 == scanf("%d%d", &a, &b))
		printf("%d\n", a+b);
}
