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
  [Tmax]	"Tmax",
  [Topenfd]	"Topenfd"
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

FFid		*lookup(uint32_t, int);
FFid		*uniqfid(void);
int		hashstr(const char*);

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
	n = convS2M(t, tbuf, msize);
	write(srvfd, tbuf, n);
	if((n = read9pmsg(srvfd, rbuf, msize)) == -1)
		errx(1, "Bad 9p read");
	convM2S(rbuf, n, r);
	if(r->tag != t->tag || r->type == Rerror || r->type != t->type+1)
		return -1;
	return 0;
}
	
int
_9pversion(uint32_t m)
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
	if(addfid("/", f) == -1)
		errx(1, "Reused fid");
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
	char	**s;

	memset(&twalk, 0, sizeof(twalk));
	f = NULL;
	twalk.type = Twalk;
	twalk.newfid = r->fid;
	while(path != NULL){
		for(s = twalk.wname; s < twalk.wname + MAXWELEM && path != NULL; s++)
			*s = strsep(&path, "/");
		_9pclunk(f);
		twalk.fid = twalk.newfid;
		f = uniqfid();
		twalk.newfid = f->fid;
		twalk.nwname = s - twalk.wname;
		if(do9p(&twalk, &rwalk) == -1)
			return NULL;
		if(rwalk.nwqid < twalk.nwname){
			lookup(f->fid, DEL);
			_9perrno = ENOENT;
			return NULL;
		}
	}
	f->qid = rwalk.wqid[rwalk.nwqid - 1];
	f->from = r;
	return f;
}

FFid*
_9pwalk(const char *path)
{
	FFid	*f;
	char	*buf, *cleanpath;

	cleanpath = estrdup(path);
	buf = estrdup(cleanname(cleanpath));
	if(strcmp(buf, "/") == 0)
		return rootfid;
	f = _9pwalkr(rootfid, buf+1);
	free(buf);
	addfid((const char*)cleanname, f);
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
	s->st_blocks = (d->length / (msize - IOHDRSZ)) + 1;
	s->st_atime = d->atime;
	s->st_mtime = s->st_ctime = d->mtime;
	s->st_rdev = 0;
	free(d);
	return 0;
}

int
_9popen(FFid *f, char mode)
{
	Fcall	topen, ropen;
	
	memset(&topen, 0, sizeof(topen));
	topen.type = Topen;
	topen.fid = f->fid;
	topen.mode = mode;
	if(do9p(&topen, &ropen) == -1)
		return -1;
	f->qid = ropen.qid;
	f->iounit = ropen.iounit;
	return 0;
}

static
long
dirpackage(uchar *buf, long ts, Dir **d)
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

long
_9pdirread(FFid *f, Dir **d)
{
	uchar	buf[DIRMAX];
	long	ts;

	ts = _9pread(f, buf, f->iounit, 0);
	if(ts >= 0)
		ts = dirpackage(buf, ts, d);
	return ts;
}

long
_9pread(FFid *f, void *buf, size_t n, off_t off)
{
	Fcall	tread, rread;

	memset(&tread, 0, sizeof(tread));
	tread.type = Tread;
	tread.fid = f->fid;
	tread.count = n;
	tread.offset = off;
	if(do9p(&tread, &rread) == -1)
		return -1;
	f->offset = rread.count + off;
	memcpy(buf, rread.data, rread.count);
	return rread.count;
}

int
_9pclunk(FFid *f)
{
	Fcall	tclunk, rclunk;

	if(f == NULL)
		return 0;
	if(lookup(f->fid, DEL) != FDEL)
		return -1;
	memset(&tclunk, 0, sizeof(tclunk));
	tclunk.type = Tclunk;
	tclunk.fid = f->fid;
	do9p(&tclunk, &rclunk);
	return 0;
}

FFid*
uniqfid(void)
{
	FFid		*f;
	uint32_t	fid;

	do
		fid = random();
	while((f = lookup(fid, PUT)) == NULL);
	return f;
}
	

FFid*
lookup(uint32_t fid, int act)
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
		if(*floc == NULL || (*floc)->path != NULL)
			return *floc;
		f = *floc;
		*floc = (*floc)->link;
		free(f);
		f = FDEL;
		break;
	}
	return f;			
}

int
str2int(const char *s)
{
	int		hash;
	const char 	*c;

	for(c = s, hash = 0; *c != '\0'; c++)
		hash += hash * 31 + *c;
	return hash >= 0 ? hash : -hash;
}

int
addfid(const char *path, FFid *f)
{
	FFid	**floc;
	char	*s;
	int	h;

	s = cleanname(estrdup(path));
	h = str2int(s);
	for(floc = pathhash + h % NHASH; *floc != NULL; floc = &(*floc)->pathlink){
		if(strcmp(s, (*floc)->path) == 0)
			break;
	}
	if(*floc != NULL)
		return -1;
	*floc = f;
	f->path = s;
	return 0;
}

FFid*
hasfid(const char *path)
{
	FFid	*f;
	char	*s;
	int	h;

	s = cleanname(estrdup(path));
	h = str2int(s);
	for(f = pathhash[h % NHASH]; f != NULL; f = f->link){
		if(strcmp(s, f->path) == 0)
			break;
	}
	free(s);
	return f;
}

FFid*
fidclone(FFid *f)
{
	Fcall	twalk, rwalk;
	FFid	*newf;

	memset(&twalk, 0, sizeof(twalk));
	twalk.type = Twalk;
	twalk.fid = f->fid;
	newf = uniqfid();
	twalk.newfid = newf->fid;
	twalk.nwname = 0;
	if(do9p(&twalk, &rwalk) != 0)
		return NULL;
	if(rwalk.nwqid != 0)
		err(1, "fidclone was not zero");
	newf->qid = *rwalk.wqid;
	return newf;
}
