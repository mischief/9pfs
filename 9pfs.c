#include <sys/types.h>
#include <sys/stdint.h>

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "util.h"

uint32_t	fid(char *);
PFid		*lookup(char*);
uint32_t	p9walk(char*);

char		*addr;
int		UFLAG;
int		TFLAG;

void	usage(void);

int
main(int argc, char *argv[])
{
	int	c;

	while((c = getopt(argc, argv, "u:t:")) != -1){
		switch(c){
		case 't':
			if(UFLAG)
				usage();
			TFLAG = 1;
			addr = optarg;
			break;
		case 'u':
			if(TFLAG)
				usage();
			UFLAG = 1;
			addr = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if(!(UFLAG || TFLAG) || argc == 0)
		usage();
	fuse_main(argc, argv, &fsops, NULL);
	exit(0);
}

void
usage(void)
{
	exit(1);
}
/*
uint32_t
fid(char *path)
{
	PFid	*f;

	f = lookup(path);
	if(f->fid == 0)
		f->fid = p9walk(path);
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

uint32_t
p9walk(char *path)
{
	struct Fcall f;
	char	*w[MAXWELEM], *p, *ap;
	size_t	siz;
	uint	nap;

	cleanname(path);
	siz = strlen(path) + 1;
	p = emalloc(len);
	strecpy(p, p + siz, path);
	f.nwname = getfields(p, f.wname, MAXWELEM, "/");
	f.type = Twalk;
	f.fid = ROOTFID;
	f.tag = newtag();
	f.newfid = newfid();
	nap = convS2M(f);
	ap = malloc(n);
	convS2M(f, ap, nap);
	write(p9fd, ap, nap);
	read9pmsg(p9fd, 
*/