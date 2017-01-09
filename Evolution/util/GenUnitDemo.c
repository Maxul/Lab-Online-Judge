#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char const *argv[])
{
	int n = atoi(argv[1]);

	srandom(time(0));
	while (n--)
		printf("%ld %ld\n", random(), random());
	return 0;
}
