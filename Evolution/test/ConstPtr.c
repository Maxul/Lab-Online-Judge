#include <stdio.h>

int main() {
	int a, b;
	while (2 == scanf("%d%d", &a, &b))
		printf("%d\n", a+b);
	char *str = "123";
	str[1] = '1';
}
