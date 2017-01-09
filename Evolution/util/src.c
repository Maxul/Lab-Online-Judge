#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>

int main()
{
	int a;
	int b;

	while (2 == scanf("%d%d", &a, &b))
		printf("%d\n", a + b);
}


