#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <err.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"

#define	 FDEL	((FFid*)~0)

char *calls2str[] = {
  [Tversion]	"Tversion",
  [Tauth]	"Tauth",
  [Tattach]	"Tattach",
  [Terror]	"Terror",
  [Tflush]	"Tflush",
  [Twalk]	"Twalk",
  [Topen]	"Topen",
  [Tcreate]	"Tcreate",
  [Tread]	"Tread",
  [Twrite]	"Twrite",
  [Tclunk]	"Tclunk",
  [Tremove]	"Tremove",
  [Tstat]	"Tstat",
  [Twstat]	"Twstat",
  [Rversion]	"Rversion",
  [Rauth]	"Rauth",
  [Rattach]	"Rattach",
  [Rerror]	"Rerror",
  [Rflush]	"Rflush",
  [Rwalk]	"Rwalk",
  [Ropen]	"Ropen",
  [Rcreate]	"Rcreate",
  [Rread]	"Rread",
  [Rwrite]	"Rwrite",
  [Rclunk]	"Rclunk",
  [Rremove]	"Rremove",
  [Rstat]	"Rstat",
  [Rwstat]	"Rwstat"
};

enum
{
	PUT,
	DEL,
	GET,
	NHASH = 1009
};

int		srvfd;
FFid		*rootfid;
void		*tbuf, *rbuf;
int		fids;
int		msize;
FFid		*fidhash[NHASH];
FFid		*pathhash[NHASH];

static FFid	*lookup(u32int, int);
static FFid	*uniqfid(void);

void
init9p(int sfd)
{
	unsigned int	seed;
	int		rfd;

	srvfd = sfd;
	if((rfd = open("/dev/random", O_RDONLY)) == -1)
		err(1, "Could not open /dev/random");
	if(read(rfd, &seed, sizeof(seed)) != sizeof(seed))
		err(1, "Bad /dev/random read");
	close(rfd);
	srandom(seed);
}

int
do9p(Fcall *t, Fcall *r)
{
	int	n;

	t->tag = random();
	if((n = convS2M(t, tbuf, msize)) == 0){
		r->ename = "Bad S2M conversion";
		goto err;
	}
	if(write(srvfd, tbuf, n) != n){
		r->ename = "Bad 9p write";
		goto err;
	}
	if((n = read9pmsg(srvfd, rbuf, msize)) == -1){
		r->ename = "Bad 9p read";
		goto err;
	}
	if(convM2S(rbuf, n, r) != n){
		r->ename = "Bad M2S conversion";
		goto err;
	}
	if(r->tag != t->tag)
		errx(1, "tag mismatch");
	if(r->type != t->type+1){
		dprint("Type mismatch\n");
		dprint("Expected %s got %s\n", calls2str[t->type], calls2str[r->type]);
		goto err;
	}
	return 0;

err:
	dprint("9p error %s\n", r->ename);
	r->type = Rerror;
	return -1;
}
	
int
_9pversion(u32int m)
{
	Fcall	tver, rver;

	memset(&tver, 0, sizeof(tver));
	tver.type = Tversion;
	tver.msize = m;
	tver.version = VERSION9P;
	msize = m;
	tbuf = erealloc(tbuf, msize);
	rbuf = erealloc(rbuf, msize);
	if(do9p(&tver, &rver) != 0)
		errx(1, "Could not establish version");
	if(rver.msize != m){
		msize = rver.msize;
		tbuf = erealloc(tbuf, msize);
		rbuf = erealloc(rbuf, msize);
	}
	return msize;
}

FFid*
_9pattach(FFid* ffid, FFid *afid)
{
	FFid		*f;
	Fcall		tattach, rattach;
	struct passwd	*pw;

	memset(&tattach, 0, sizeof(tattach));
	if((pw = getpwuid(getuid())) == NULL)
		errx(1, "Could not get user");
	tattach.type = Tattach;
	tattach.fid = ffid->fid;
	tattach.afid = afid->fid;
	tattach.uname = pw->pw_name;
	tattach.msize = msize;
	if(do9p(&tattach, &rattach) != 0)
		errx(1, "Could not attach");
	f = lookup(ffid->fid, PUT);
	f->path = "/";
	f->fid = tattach.fid;
	f->qid = rattach.qid;
	rootfid = f;
	return f;
}	

FFid*
_9pwalkr(FFid *r, char *path)
{
	FFid	*f;
	Fcall	twalk, rwalk;
	char	**s, *buf, *bp;

	buf = bp = estrdup(path);
	memset(&twalk, 0, sizeof(twalk));
	f = NULL;
	twalk.type = Twalk;
	twalk.newfid = r->fid;
	while(buf != NULL){
		for(s = twalk.wname; s < twalk.wname + MAXWELEM && buf != NULL; s++)
			*s = strsep(&buf, "/");
		_9pclunk(f);
		twalk.fid = twalk.newfid;
		f = uniqfid();
		twalk.newfid = f->fid;
		twalk.nwname = s - twalk.wname;
		if(do9p(&twalk, &rwalk) == -1 || rwalk.nwqid < twalk.nwname){
			if(lookup(f->fid, DEL) != FDEL)
				errx(1, "Fid %d not found in hash", f->fid);
			free(bp);
			return NULL;
		}
	}
	f->qid = rwalk.wqid[rwalk.nwqid - 1];
	free(bp);
	return f;
}

FFid*
_9pwalk(const char *path)
{
	FFid	*f;
	char	*cleanpath;

	cleanpath = cleanname(estrdup(path));
	if(strcmp(cleanpath, "/") == 0){
		free(cleanpath);
		return fidclone(rootfid);
	}
	if((f = _9pwalkr(rootfid, cleanpath+1)) != NULL)
		f->path = cleanpath;
	return f;
}

int
_9pstat(FFid *f, struct stat *s)
{
	Dir		*d;
	struct passwd	*p;
	struct group	*g;
	Fcall		tstat, rstat;

	memset(&tstat, 0, sizeof(tstat));
	tstat.type = Tstat;
	tstat.fid = f->fid;
	if(do9p(&tstat, &rstat) != 0)
		return -1;
	d = emalloc(sizeof(*d) + rstat.nstat);
	if(convM2D(rstat.stat, rstat.nstat, d, (char*)(d+1)) != rstat.nstat){
		free(d);
		return -1;
	}
	s->st_dev = d->dev;
	s->st_ino = d->qid.path;
	s->st_mode = d->mode & 0777;
	if(d->mode & DMDIR)
		s->st_mode |= S_IFDIR;
	else
		s->st_mode |= S_IFREG;
	s->st_nlink = d->mode & DMDIR ? rstat.nstat + 1 : 1;
	s->st_uid = (p = getpwnam(d->uid)) == NULL ? 0 : p->pw_uid;
	s->st_gid = (g = getgrnam(d->gid)) == NULL ? 0 : g->gr_gid;
	s->st_size = d->length;
	s->st_blksize = msize - IOHDRSZ;
	s->st_blocks = d->length / (msize - IOHDRSZ) + 1;
	s->st_atime = d->atime;
	s->st_mtime = s->st_ctime = d->mtime;
	s->st_rdev = 0;
	free(d);
	return 0;
}

int
_9popen(FFid *f)
{
	Fcall	topen, ropen;
	
	memset(&topen, 0, sizeof(topen));
	topen.type = Topen;
	topen.fid = f->fid;
	topen.mode = f->mode;
	if(do9p(&topen, &ropen) == -1)
		return -1;
	f->qid = ropen.qid;
	if(ropen.iounit != 0)
		f->iounit = ropen.iounit;
	else
		f->iounit = msize - IOHDRSZ;
	return 0;
}

FFid*
_9pcreate(FFid *f, char *name, int perm)
{
	Fcall	tcreate, rcreate;

	perm &= 0777;
	memset(&tcreate, 0, sizeof(tcreate));
	tcreate.type = Tcreate;
	tcreate.fid = f->fid;
	tcreate.name = name;
	tcreate.perm = perm;
	tcreate.mode = f->mode;
	if(do9p(&tcreate, &rcreate) == -1){
		_9pclunk(f);
		return NULL;
	}
	if(rcreate.iounit != 0)
		f->iounit = rcreate.iounit;
	else
		f->iounit = msize - IOHDRSZ;
	f->qid = rcreate.qid;
	return f;
}

int
_9premove(FFid *f)
{
	Fcall	tremove, rremove;

	if(f == NULL)
		return 0;
	memset(&tremove, 0, sizeof(tremove));
	tremove.type = Tremove;
	tremove.fid = f->fid;
	if(do9p(&tremove, &rremove) == -1){
		_9pclunk(f);
		return -1;
	}
	if(lookup(f->fid, DEL) != FDEL)
		errx(1, "Fid %d not found in hash", f->fid);
	return 0;
}

static
u32int
dirpackage(uchar *buf, u32int ts, Dir **d)
{
	char *s;
	long ss, i, n, nn, m;

	*d = nil;
	if(ts <= 0)
		return 0;

	/*
	 * first find number of all stats, check they look like stats, & size all associated strings
	 */
	ss = 0;
	n = 0;
	for(i = 0; i < ts; i += m){
		m = BIT16SZ + GBIT16(&buf[i]);
		if(statcheck(&buf[i], m) < 0)
			break;
		ss += m;
		n++;
	}

	if(i != ts)
		return -1;

	*d = malloc(n * sizeof(Dir) + ss);
	if(*d == nil)
		return -1;

	/*
	 * then convert all buffers
	 */
	s = (char*)*d + n * sizeof(Dir);
	nn = 0;
	for(i = 0; i < ts; i += m){
		m = BIT16SZ + GBIT16((uchar*)&buf[i]);
		if(nn >= n || convM2D(&buf[i], m, *d + nn, s) != m){
			free(*d);
			*d = nil;
			return -1;
		}
		nn++;
		s += m;
	}

	return nn;
}

int
_9pdirread(FFid *f, Dir **d)
{
	uchar	buf[DIRMAX];
	u32int	ts, n;

	n = f->iounit;
	ts = _9pread(f, buf, n);
	if(ts >= 0)
		ts = dirpackage(buf, ts, d);
	return ts;
}

int
_9pread(FFid *f, void *buf, u32int n)
{
	Fcall	tread, rread;

	memset(&tread, 0, sizeof(tread));
	tread.type = Tread;
	tread.fid = f->fid;
	tread.offset = f->offset;
	tread.count = n < f->iounit ? n : f->iounit;
	dprint("_9pread on %s with count %u, offset %lld, fid %u\n", f->path, tread.count, tread.offset, f->fid);
	if(do9p(&tread, &rread) == -1){
		dprint("_9pread error returned from do9p\n");
		return -1;
	}
	f->offset += rread.count;
	dprint("Data returned was\n%s with count %d\n", rread.data, rread.count);
	memcpy(buf, rread.data, rread.count);
	dprint("_9pread returning file offset is %lu, fid for file is %u\n", f->offset, f->fid);
	return rread.count;
}

int
_9pwrite(FFid *f, void *buf, u32int n)
{
	Fcall	twrite, rwrite;

	memset(&twrite, 0, sizeof(twrite));
	twrite.type = Twrite;
	twrite.fid = f->fid;
	twrite.offset = f->offset;
	twrite.count = n < f->iounit ? n : f->iounit;
	twrite.data = buf;
	if(do9p(&twrite, &rwrite) == -1)
		return -1;
	f->offset += rwrite.count;
	return rwrite.count;
}

int
_9pclunk(FFid *f)
{
	Fcall	tclunk, rclunk;

	if(f == NULL)
		return 0;
	memset(&tclunk, 0, sizeof(tclunk));
	tclunk.type = Tclunk;
	tclunk.fid = f->fid;
	if(lookup(f->fid, DEL) != FDEL)
		errx(1, "Fid %d not found in hash", f->fid);
	return do9p(&tclunk, &rclunk);
}

static
FFid*
uniqfid(void)
{
	FFid	*f;
	u32int	fid;

	do
		fid = random();
	while((f = lookup(fid, PUT)) == NULL);
	return f;
}
	
static
FFid*
lookup(u32int fid, int act)
{
	FFid	**floc, *f;

	f = NULL;
	for(floc = fidhash + fid % NHASH; *floc != NULL; floc = &(*floc)->link){
		if((*floc)->fid == fid)
			break;
	}
	switch(act){
	case GET:
		f = *floc;
		break;
	case PUT:
		if(*floc != NULL)
			return NULL;
		f = emalloc(sizeof(*f));
		f->fid = fid;
		*floc = f;
		break;
	case DEL:
		if(*floc == NULL || *floc == rootfid)
			return NULL;
		f = *floc;
		*floc = (*floc)->link;
		if(f->path != NULL)
			free(f->path);
		free(f);
		f = FDEL;
		break;
	}
	return f;			
}

FFid*
fidclone(FFid *f)
{
	Fcall	twalk, rwalk;
	FFid	*newf;

	newf = uniqfid();
	memset(&twalk, 0, sizeof(twalk));
	twalk.type = Twalk;
	twalk.fid = f->fid;
	twalk.newfid = newf->fid;
	twalk.nwname = 0;
	if(do9p(&twalk, &rwalk) != 0)
		return NULL;
	if(rwalk.nwqid != 0)
		err(1, "fidclone was not zero");
	newf->qid = *rwalk.wqid;
	return newf;
}
