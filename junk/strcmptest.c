#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int
main(void)
{
	printf("%d\n", strcmp("", ""));
	printf("%d\n", strcmp("a", ""));
	printf("%d\n", strcmp("", "a"));
	exit(0);
}