#include <sys/types.h>

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "util.h"

uint32_t	fid(char *);
PFid		*lookup(char*);
uint32_t	p9walk(uint32_t*, char*);

int
main(void)
{
	exit(0);
}

uint32_t
fid(char *path)
{
	PFid	*f;

	f = lookup(path);
	if(f->fid == 0)
		f->fid = p9walk(&f->fid, path);
	return f->fid;
}

PFid*
lookup(char *path)
{
	PFid	*f;
	int	siz, h;

	h = hash(path);
	for(f = fidhash[h]; f != NULL; f = f->link){
		if(strcmp(f->path, path) == 0)
			break;
	}
	if(f == NULL){
		f = emalloc(sizeof(*f));
		siz = strlen(path) + 1;
		f->path = emalloc(siz);
		strecpy(f->path, f->path + siz, path);
		f->link = fidhash[h];
		fidhash[h] = f;
	}
	return f;
}

