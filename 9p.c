#include <sys/types.h>
#include <sys/stdint.h>

#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "9p.h"
#include "util.h"

FFid	*rootfid;
char	*tbuf;
char	*rbuf;
int	msize;

void
init9p(int m)
{
	if((msize = _9pversion(srvfd, m) <= 0))
		errx(1, "Bad msize");
	tbuf = erealloc(tbuf, msize);
	rbuf = erealloc(rbuf, msize);
	if((rootfid = _9pattach(srvfd, 0, (uint16_t)~0)) == NULL)
		errx(1, "Could not attach.");
}

int
do9p(Fcall *t, Fcall *r)
{
	int	n;

	n = convS2M(t, tbuf, msize);
	write(srvfd, tbuf, n);
	if((n = read9pmsg(srvfd, rbuf, msize)) == -1)
		errx(1, "Bad 9p read.");
	convM2S(rbuf, n, r);
	if(r->type == Rerror)
		return RERROR;
	if(r->type != t->type+1)
		return RMISS;
	return 0;
}
	
int
_9pversion(int fd, uint32_t m)
{
	Fcall	tver, rver;

	tbuf = emalloc(m);
	rbuf = emalloc(m);
	tver.type = Tversion;
	tver.tag = (ushort)~0;
	tver.msize = m;
	tver.version = VERSION9P;

	if(do9p(&tver, &rver) != 0)
		errx(1, "Could not establish version.");
	return rver.msize;
}

FFid*
_9pattach(int fd, uint16_t fid, uint16_t afid)
{
	return NULL;
}	
