#include <sys/types.h>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pwd.h>
#include <err.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "9p.h"
#include "util.h"

FFid	*rootfid;
char	*tbuf;
char	*rbuf;
int	fids;
int	msize;

void
init9p(int m)
{
	if((msize = _9pversion(srvfd, m) <= 0))
		errx(1, "Bad msize");
	tbuf = emalloc(msize);
	rbuf = emalloc(msize);
	if((rootfid = _9pattach(srvfd, 0, NOFID)) == NULL)
		errx(1, "Could not attach");
}

int
do9p(Fcall *t, Fcall *r, char *tb, char *rb)
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
	char	*t, *r;

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
	f = lookup(0);
	f->path = "/";
	f->fid = tattach.fid;
	f->qid = rattach.qid;
	return f;
}	

int
_9pwalk(char *path, FFid *f)
{
	Fcall	twalk, rwalk;
	char	*cpath, *buf, *p;
	uin32_t	oldfid;
	int	nwalk, i;

	memset(twalk, 0, sizeof(twalk));
	clunkme = 0;
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
		_9pclunk(oldfid);
		oldfid = twalk.newfid;
		twalk.type = Twalk;
		twalk.fid = nwalk ? rootfid : twalk.newfid;
		twalk.newfid = uniqfid();
		twalk.nwname = i;
		if(do9p(&twalk, &rwalk, tbuf, rbuf) != 0){
			free(buf);
			return -1
		}
		if(rwalk.nwqid != twalk.nwname)
			return -ENOENT;
	}
	f->path = cleanname(estrdup(path));
	f->fid = twalk.newfid;
	f->qid = rwalk.wqid[rwalk.nwqid - 1];
	f->offset = 0;
	return 0;
}

int
_9pclunk(uint32_t fid)
{
	return 0;
}
