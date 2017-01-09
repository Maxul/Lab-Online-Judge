#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main() {
	int a, b;
	while (2 == scanf("%d%d", &a, &b))
		printf("%d\n", a+b);
	getpid();
}
