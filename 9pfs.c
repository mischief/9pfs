#include <sys/types.h>

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dat.h"
#include "fcall.h"
#include "libc.h"
#include "util.h"

uint32_t	fid(char *);
PPFid		*lookup(char*);

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
	if(f->fid == nil)
		f->fid = p9walk(&f->fid, path);
	return f->fid;
}

PFid
lookup(char *path)
{
	PFid	*f;
	int	siz, h;

	h = hash(path);
	for(f = fidhash[h]; f != nil; f = f->link){
		if(strcmp(f->path, path) == 0)
			break;
	}
	if(f == nil){
		f = emalloc(sizeof(*f));
		siz = strlen(path) + 1;
		f->path = emalloc(siz);
		strecpy(f->path, f->path + siz, path);
		f->link = fidhash[h];
		fidhash[h] = f;
	}
	return f;
}

