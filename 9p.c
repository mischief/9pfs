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
#include "util.h"

#define	 FDEL	((void*)~0)

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

FFid	*rootfid;
FFid	*authfid;
void	*tbuf, *rbuf;
int	msize;
FFid	*fidhash[NHASH];
FDir	*dirhash[NHASH];

FFid	*lookupfid(u32int, int);
FFid	*uniqfid(void);
FDir	*lookupdir(char*, int);

void
init9p(void)
{
	unsigned int	seed;
	int		rfd;

	if((rfd = open("/dev/random", O_RDONLY)) == -1)
		err(1, "Could not open /dev/random");
	if(read(rfd, &seed, sizeof(seed)) != sizeof(seed))
		err(1, "Bad /dev/random read");
	close(rfd);
	srandom(seed);
}

int
do9p(int srvfd, Fcall *t, Fcall *r)
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
_9pversion(int fd, u32int m)
{
	Fcall	tver, rver;

	memset(&tver, 0, sizeof(tver));
	tver.type = Tversion;
	tver.msize = m;
	tver.version = VERSION9P;
	msize = m;
	tbuf = erealloc(tbuf, msize);
	rbuf = erealloc(rbuf, msize);
	if(do9p(fd, &tver, &rver) != 0)
		errx(1, "Could not establish version");
	if(rver.msize != m){
		msize = rver.msize;
		tbuf = erealloc(tbuf, msize);
		rbuf = erealloc(rbuf, msize);
	}
	return msize;
}

FFid*
_9pauth(int fd, FFid *afid, char *aname)
{
	FFid	*f;
	Fcall	tauth, rauth;
	struct passwd	*pw;

	dprint("_9pauth on %d\n", afid->fid);
	memset(&tauth, 0, sizeof(tauth));
	if((pw = getpwuid(getuid())) == NULL)
		errx(1, "Could not get user");
	tauth.type = Tauth;
	tauth.afid = afid->fid;
	tauth.uname = pw->pw_name;
	tauth.aname = aname;
	if(do9p(fd, &tauth, &rauth) == -1)
		errx(1, "Could not auth");
	f = lookupfid(afid->fid, PUT);
	f->path = "AUTHFID";
	f->fid = afid->fid;
	f->qid = rauth.aqid;
	authfid = f;
	return f;
}

FFid*
_9pattach(int fd, FFid* ffid, FFid *afid)
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
	if(do9p(fd, &tattach, &rattach) == -1)
		errx(1, "Could not attach");
	f = lookupfid(ffid->fid, PUT);
	f->path = "/";
	f->fid = ffid->fid;
	f->qid = rattach.qid;
	rootfid = f;
	return f;
}	

FFid*
_9pwalkr(int fd, FFid *r, char *path)
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
		_9pclunk(fd, f);
		twalk.fid = twalk.newfid;
		f = uniqfid();
		twalk.newfid = f->fid;
		twalk.nwname = s - twalk.wname;
		if(do9p(fd, &twalk, &rwalk) == -1 || rwalk.nwqid < twalk.nwname){
			if(lookupfid(f->fid, DEL) != FDEL)
				errx(1, "Fid %d not found in hash", f->fid);
			free(bp);
			dprint("_9pwalkr path not found %s\n", path);
			return NULL;
		}
	}
	f->qid = rwalk.wqid[rwalk.nwqid - 1];
	free(bp);
	return f;
}

FFid*
_9pwalk(int fd, const char *path)
{
	FFid	*f;
	char	*pnew;

	pnew = estrdup(path);
	if(strcmp(pnew, "/") == 0){
		free(pnew);
		return fidclone(fd, rootfid);
	}
	if((f = _9pwalkr(fd, rootfid, pnew+1)) == NULL){
		free(pnew);
		return NULL;
	}
	f->path = pnew;
	return f;
}

void
dir2stat(struct stat *s, Dir *d)
{
	struct passwd	*p;
	struct group	*g;

	s->st_dev = d->dev;
	s->st_ino = d->qid.path;
	s->st_mode = d->mode & 0777;
	if(d->mode & DMDIR)
		s->st_mode |= S_IFDIR;
	else
		s->st_mode |= S_IFREG;
	s->st_nlink = 1;
	s->st_uid = (p = getpwnam(d->uid)) == NULL ? 0 : p->pw_uid;
	s->st_gid = (g = getgrnam(d->gid)) == NULL ? 0 : g->gr_gid;
	s->st_size = d->length;
	s->st_blksize = msize - IOHDRSZ;
	s->st_blocks = d->length / (msize - IOHDRSZ) + 1;
	s->st_atime = d->atime;
	s->st_mtime = s->st_ctime = d->mtime;
	s->st_rdev = 0;
}	

Dir*
_9pstat(int fd, FFid *f)
{
	Dir	*d;
	Fcall	tstat, rstat;

	memset(&tstat, 0, sizeof(tstat));
	tstat.type = Tstat;
	tstat.fid = f->fid;
	if(do9p(fd, &tstat, &rstat) == -1)
		return NULL;
	d = emalloc(sizeof(*d) + rstat.nstat);
	if(convM2D(rstat.stat, rstat.nstat, d, (char*)(d+1)) != rstat.nstat){
		free(d);
		return NULL;
	}
	return d;
}

int
_9pwstat(int fd, FFid *f, Dir *d)
{
	Fcall	twstat, rwstat;
	uchar	*st;
	int	n;

	n = sizeD2M(d);
	st = emalloc(n);
	if(convD2M(d, st, n) != n){
		free(st);
		return -1;
	}
	memset(&twstat, 0, sizeof(twstat));
	twstat.type = Twstat;
	twstat.fid = f->fid;
	twstat.nstat = n;
	twstat.stat = st;
	if(do9p(fd, &twstat, &rwstat) == -1){
		free(st);
		return -1;
	}
	free(st);
	return 0;
}

int
_9popen(int fd, FFid *f)
{
	Fcall	topen, ropen;
	
	memset(&topen, 0, sizeof(topen));
	topen.type = Topen;
	topen.fid = f->fid;
	topen.mode = f->mode;
	if(do9p(fd, &topen, &ropen) == -1)
		return -1;
	f->qid = ropen.qid;
	if(ropen.iounit != 0)
		f->iounit = ropen.iounit;
	else
		f->iounit = msize - IOHDRSZ;
	return 0;
}

FFid*
_9pcreate(int fd, FFid *f, char *name, int perm, int isdir)
{
	Fcall	tcreate, rcreate;

	perm &= 0777;
	if(isdir)
		perm |= DMDIR;
	memset(&tcreate, 0, sizeof(tcreate));
	tcreate.type = Tcreate;
	tcreate.fid = f->fid;
	tcreate.name = name;
	tcreate.perm = perm;
	tcreate.mode = f->mode;
	if(do9p(fd, &tcreate, &rcreate) == -1){
		_9pclunk(fd, f);
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
_9premove(int fd, FFid *f)
{
	Fcall	tremove, rremove;

	if(f == NULL)
		return 0;
	memset(&tremove, 0, sizeof(tremove));
	tremove.type = Tremove;
	tremove.fid = f->fid;
	if(do9p(fd, &tremove, &rremove) == -1){
		_9pclunk(fd, f);
		return -1;
	}
	if(lookupfid(f->fid, DEL) != FDEL)
		errx(1, "Fid %d not found in hash", f->fid);
	return 0;
}

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
_9pdirread(int fd, FFid *f, Dir **d)
{
	FDir	*fdir;
	uchar	buf[DIRMAX], *b;
	u32int	ts, n;

	dprint("_9pdirread\n");
	n = f->iounit;
	b = buf;
	while((ts = _9pread(fd, f, b, n)) > 0){
		b += ts;
		if(b - buf > DIRMAX - n)
			break;
	}
	if(ts < 0)
		return ts;
	ts = dirpackage(buf, b - buf, d);
	dprint("_9pdirread about to lookupdir with path %s\n", f->path);
	if((fdir = lookupdir(f->path, PUT)) != NULL){
		fdir->dirs = *d;
		fdir->ndirs = ts;
		dprint("_9pdirread new FDir with ndirs %d\n", fdir->ndirs);
	}
	return ts;
}

int
_9pread(int fd, FFid *f, void *buf, u32int n)
{
	Fcall	tread, rread;

	memset(&tread, 0, sizeof(tread));
	tread.type = Tread;
	tread.fid = f->fid;
	tread.offset = f->offset;
	tread.count = n < f->iounit ? n : f->iounit;
	dprint("_9pread on %s with count %u, offset %lld, fid %u\n", f->path, tread.count, tread.offset, f->fid);
	if(do9p(fd, &tread, &rread) == -1){
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
_9pwrite(int fd, FFid *f, void *buf, u32int n)
{
	Fcall	twrite, rwrite;

	memset(&twrite, 0, sizeof(twrite));
	twrite.type = Twrite;
	twrite.fid = f->fid;
	twrite.offset = f->offset;
	twrite.count = n < f->iounit ? n : f->iounit;
	twrite.data = buf;
	if(do9p(fd, &twrite, &rwrite) == -1)
		return -1;
	f->offset += rwrite.count;
	return rwrite.count;
}

int
_9pclunk(int fd, FFid *f)
{
	Fcall	tclunk, rclunk;

	if(f == NULL)
		return 0;
	memset(&tclunk, 0, sizeof(tclunk));
	tclunk.type = Tclunk;
	tclunk.fid = f->fid;
	if(lookupfid(f->fid, DEL) != FDEL)
		errx(1, "Fid %d not found in hash", f->fid);
	return do9p(fd, &tclunk, &rclunk);
}

FFid*
uniqfid(void)
{
	FFid	*f;
	u32int	fid;

	do
		fid = random();
	while((f = lookupfid(fid, PUT)) == NULL);
	return f;
}
	

FFid*
lookupfid(u32int fid, int act)
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
		*floc = f->link;
		free(f->path);
		free(f);
		f = FDEL;
		break;
	}
	return f;			
}

FFid*
fidclone(int fd, FFid *f)
{
	Fcall	twalk, rwalk;
	FFid	*newf;

	newf = uniqfid();
	memset(&twalk, 0, sizeof(twalk));
	twalk.type = Twalk;
	twalk.fid = f->fid;
	twalk.newfid = newf->fid;
	twalk.nwname = 0;
	if(do9p(fd, &twalk, &rwalk) == -1){
		lookupfid(f->fid, DEL);
		return NULL;
	}
	if(rwalk.nwqid != 0)
		err(1, "fidclone was not zero");
	newf->qid = *rwalk.wqid;
	newf->path = estrdup(f->path);
	return newf;
}

uint
strkey(char *s)
{
	char	*p;
	uint	h;

	h = 0;
	for(p = s; *p != '\0'; p++)
		h = h*31 + *p;
	return h;
}

FDir*
lookupdir(char *path, int act)
{
	FDir	**fdloc, *fd;
	uint	h;

	fd = NULL;
	h = strkey(path);
	for(fdloc = dirhash + h % NHASH; *fdloc != NULL; fdloc = &(*fdloc)->link){
		if(strcmp((*fdloc)->path, path) == 0)
			break;
	}
	switch(act){
	case GET:
		fd = *fdloc;
		break;
	case PUT:
		if(*fdloc != NULL){
			fd = *fdloc;
			free(fd->dirs);
			fd->dirs = NULL;
			fd->ndirs = 0;
			dprint("lookupdir update fd with path %s\n", fd->path);
		}else{
			fd = emalloc(sizeof(*fd));
			fd->path = estrdup(path);
			*fdloc = fd;
			dprint("lookupdir new fd with path %s\n", fd->path);
		}
		break;
	case DEL:
		if(*fdloc == NULL)
			return NULL;
		fd = *fdloc;
		*fdloc = fd->link;
		free(fd->path);
		free(fd->dirs);
		free(fd);
		fd = FDEL;
		break;
	}
	return fd;
}

Dir*
isdircached(const char *path)
{
	FDir	*fd;
	Dir	*d;
	char	*dname, *bname;

	dname = estrdup(path);
	bname = strrchr(dname, '/');
	*bname++ = '\0';
	dprint("getstat dname is %s bname is %s\n", dname, bname);
	if((fd = lookupdir(dname, GET)) == NULL){
		free(dname);
		return NULL;
	}
	dprint("getstat fd found, path is %s, ndirs is %d\n", fd->path, fd->ndirs);
	for(d = fd->dirs; d < fd->dirs + fd->ndirs; d++){
		if(strcmp(d->name, bname) == 0)
			break;
	}
	if(d == fd->dirs + fd->ndirs){
		dprint("getstat bname %s not found\n", bname);
		free(dname);
		return NULL;
	}
	dprint("getstat path given was %s, dir found was %s\n", path, d->name);
	free(dname);
	return d;
}

int
fauth(int fd, char *aname)
{
	char	fbuf[1024];
	int	r, pid, p[2];
	FFid	afid, *af;

	afid.fid = AUTHFID;
	af = _9pauth(fd, &afid, aname);
	if(pipe(p) == -1)
		err(1, "fauth could not create pipe");

	if((pid = fork()) == -1)
		err(1, "Could not fork");
	if(pid > 0){
		close(p[1]);
		return p[0];
	}
	close(p[0]);
	while((r = read(p[1], fbuf, sizeof(fbuf))) > 0){
		dprint("fauth read %s\n", fbuf);
		if(_9pwrite(fd, af, fbuf, r) < 0)
			err(1, "fauth 9pwrite error");
		if((r = _9pread(fd, af, fbuf, sizeof(fbuf))) < 0)
			err(1, "fauth 9pread error");
		if(write(p[1], fbuf, r) < 0)
			err(1, "fauth write error");
	}
	dprint("fauth exiting %d\n", r);
	if(r < 0)
		err(1, "fauth read error");
	exit(0);
}
