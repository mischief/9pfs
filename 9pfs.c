#include <sys/types.h>

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "dat.h"

uint32_t fid(char *path);

int
main(void)
{
	exit(0);
}

uint32_t
fid(char *path)
{
	uint32_t	f;
	Fidl		fl;

	fl = lookup(*path);
	if(fl->fid == nil)
		fl->fid = 