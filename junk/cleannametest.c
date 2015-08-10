#include <sys/types.h>

#include <err.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "../libc.h"

int
main(int argc, char *argv[])
{
	if(argc < 2)
		errx(1, "provide an argument");
	printf("%s\n", cleanname(argv[1]));
	exit(0);
}
