#include <sys/types.h>
#include <sys/stdint.h>

#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "util.h"

char	*tbuf;
char	*rbuf;
int	msize;

void
init9p(int m)
{
	tbuf = emalloc(m);
	rbuf = emalloc(m);
	msize = m;
}

int
do9p(Fcall *t, Fcall *r)
{
	int	n;
	char	*tbuf, *rbuf;

	n = sizeS2M(t);
	tbuf = emalloc(n);
	if(convS2M(t, tbuf, n) != n){
		free(tbuf);
		errx(1, "Bad convS2M conversion");
	}
	write(srvfd, tbuf, n);
/*	read(srvfd, rbuf, n); */
}
	
int
_9pversion(int fd, uint32_t *m)
{
	Fcall	vcall;
	char	*buf;
	int	s;

	buf = emalloc(*m);
	vcall.type = Tversion;
	vcall.tag = (ushort)~0;
	vcall.msize = *m;
	vcall.version = VERSION9P;

	s = sizeS2M(&vcall);
	if(convS2M(&vcall, buf, s) != s)
		errx(1, "Bad Fcall conversion.");
	write(fd, buf, s);
	s = read(fd, buf, *m);
	convM2S(buf, s, &vcall);
	*m = vcall.msize;
	return s;
}

FFid*
_9pattach(int fd, uint16_t fid, uint16_t afid)
{
	return NULL;
}	
