#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <err.h>
#include <errno.h>

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"

#define	 FDEL	((FFid*)~0)

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

	n = convS2M(t, tbuf, msize);
	write(srvfd, tbuf, n);
	if((n = read9pmsg(srvfd, rbuf, msize)) == -1)
		errx(1, "Bad 9p read");
	convM2S(rbuf, n, r);
	if(r->type == Rerror || r->type != t->type+1)
		return -1;
	return 0;
}
	
int
_9pversion(uint32_t m)
{
	Fcall	tver, rver;

	tver.type = Tversion;
	tver.tag = 0;
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
	return f;
}	

FFid*
_9pwalk(const char *path)
{
	FFid	*f;
	Fcall	twalk, rwalk;
	char	*curpath, *buf;
	int	nwalk, i;

	f = NULL;
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
		_9pclunk(f);
		twalk.type = Twalk;
		twalk.fid = nwalk ? rootfid->fid : twalk.newfid;
		f = uniqfid();
		twalk.newfid = f->fid;
		twalk.nwname = i;
		if(do9p(&twalk, &rwalk) != 0){
			free(buf);
			_9perrno = -1;
			return NULL;
		}
		if(rwalk.nwqid != twalk.nwname){
			lookup(f->fid, DEL);
			_9perrno = ENOENT;
			return NULL;
		}
	}
	addfid(path, f);
	f->qid = rwalk.wqid[rwalk.nwqid - 1];
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
_9popen(FFid *f)
{
	return 0;
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
	return 0;
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
	newf->qid = *rwalk.wqid;
	return newf;
}
