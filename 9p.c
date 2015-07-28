#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "9p.h"
#include "util.h"

enum
{
	PUT,
	DEL,
	GET,
	NHASH = 1009
};

FFid		*rootfid;
void		*tbuf, *rbuf;
int		fids;
int		msize;
FFid		*fidhash[NHASH];

FFid		*lookup(uint32_t, int);
uint32_t	uniqfid(void);

void
init9p(int m)
{
	unsigned int	seed;
	int		rfd;

	if((rfd = open("/dev/random", O_RDONLY)) == -1)
		err(1, "Could not open /dev/random");
	if(read(rfd, &seed, sizeof(seed)) != sizeof(seed))
		err(1, "Bad /dev/random read");
	close(rfd);
	srandom(seed);

	if((msize = _9pversion(srvfd, m) <= 0))
		errx(1, "Bad msize");
	tbuf = emalloc(msize);
	rbuf = emalloc(msize);
	if((rootfid = _9pattach(srvfd, 0, NOFID)) == NULL)
		errx(1, "Could not attach");
}

int
do9p(Fcall *t, Fcall *r, uchar *tb, uchar *rb)
{
	int	n;

	n = convS2M(t, tb, msize);
	write(srvfd, tb, n);
	if((n = read9pmsg(srvfd, rb, msize)) == -1)
		errx(1, "Bad 9p read.");
	convM2S(rb, n, r);
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
	void	*t, *r;

	t = emalloc(m);
	r = emalloc(m);
	tver.type = Tversion;
	tver.tag = 0;
	tver.msize = m;
	tver.version = VERSION9P;

	if(do9p(&tver, &rver, t, r) != 0)
		errx(1, "Could not establish version");
	free(t);
	free(r);
	return rver.msize;
}

FFid*
_9pattach(int fd, uint32_t fid, uint32_t afid)
{
	FFid		*f;
	Fcall		tattach, rattach;
	struct passwd	*pw;

	if((pw = getpwuid(getuid())) == NULL)
		errx(1, "Could not get user");
	tattach.type = Tattach;
	tattach.tag = 0;
	tattach.fid = fid;
	tattach.afid = afid;
	tattach.uname = pw->pw_name;
	tattach.msize = msize;
	if(do9p(&tattach, &rattach, tbuf, rbuf) != 0)
		errx(1, "Could not attach");
	f = lookup(0, PUT);
	f->path = "/";
	f->fid = tattach.fid;
	f->qid = rattach.qid;
	return f;
}	

FFid*
_9pwalk(const char *path)
{
	FFid	*f;
	Fcall	twalk, rwalk;
	char	*curpath, *buf;
	int	nwalk, i;

	
	memset(&twalk, 0, sizeof(twalk));
	buf = estrdup(path);
	cleanname(buf);
	curpath = buf;
	for(nwalk = 0; curpath != NULL && *curpath != '\0'; nwalk++){
		for(i = 0; i < MAXWELEM && *curpath != '\0'; ){
			twalk.wname[i++] = curpath;
			if((curpath = strchr(curpath, '/')) == NULL)
				break;
			*curpath++ = '\0';
		}
		_9pclunk(lookup(twalk.fid, DEL));
		twalk.type = Twalk;
		twalk.fid = nwalk ? rootfid->fid : twalk.newfid;
		twalk.newfid = uniqfid();
		twalk.nwname = i;
		if(do9p(&twalk, &rwalk, tbuf, rbuf) != 0){
			free(buf);
			_9perrno = -1;
			return NULL;
		}
		if(rwalk.nwqid != twalk.nwname){
			_9perrno = ENOENT;
			return NULL;
		}
	}
	f = lookup(twalk.newfid, GET);
	f->path = cleanname(estrdup(path));
	f->qid = rwalk.wqid[rwalk.nwqid - 1];
	return f;
}

int
_9pstat(FFid *f, struct stat *s)
{
	Dir	*d;
	Fcall	t, r;

	t.type = Tstat;
	t.fid = f->fid;
	t.tag = 0;
	if(do9p(&t, &r, tbuf, rbuf) != 0)
		return -1;
	d = emalloc(sizeof(*d) + r.nstat);
	if(convM2D(r.stat, r.nstat, d, (char*)(d+1)) != r.nstat){
		free(d);
		return -1;
	}
	s->st_dev = d->dev;
	s->st_ino = d->qid.path;
	s->st_mode = d->mode;
	s->st_nlink = d->mode & DMDIR ? r.nstat + 1 : 1;
	return 0;
}

int
_9pclunk(FFid *f)
{
	return 0;
}

uint32_t
uniqfid(void)
{
	uint32_t	fid;

	do
		fid = random();
	while(lookup(fid, PUT) == NULL);
	return fid;
}
	

FFid*
lookup(uint32_t fid, int act)
{
	FFid **floc, *f;

	f = NULL;
	floc = fidhash + fid % NHASH;
	while(*floc != NULL){
		if((*floc)->fid == fid)
			break;
		floc = &(*floc)->link;
	}
	switch(act){
	case DEL:
	case GET:
		if(*floc == NULL)
			break;
		f = *floc;
		if(act == DEL && fid != 0)
			*floc = (*floc)->link;
		break;
	case PUT:
		if(*floc != NULL)
			break;
		f = emalloc(sizeof(*f));
		f->fid = fid;
		*floc = f;
		break;
	}
	return f;			
}
