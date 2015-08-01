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
PFid		*pathhash[NHASH];

FFid		*lookup(uint32_t, int);
FFid		*uniqfid(void);
int		hashstr(const char*);

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

	if((msize = _9pversion(m) <= 0))
		errx(1, "Bad msize");
	tbuf = emalloc(msize);
	rbuf = emalloc(msize);
	if((rootfid = _9pattach(0, NOFID)) == NULL)
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
_9pversion(uint32_t m)
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
_9pattach(uint32_t fid, uint32_t afid)
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
		_9pclunk(twalk.fid);
		twalk.type = Twalk;
		twalk.fid = nwalk ? rootfid->fid : twalk.newfid;
		f = uniqfid();
		twalk.newfid = f->fid;
		twalk.nwname = i;
		if(do9p(&twalk, &rwalk, tbuf, rbuf) != 0){
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
	Fcall		t, r;

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
	s->st_mode = d->mode & 0777;
	if(d->mode & DMDIR)
		s->st_mode |= S_IFDIR;
	else
		s->st_mode |= S_IFREG;
	s->st_nlink = d->mode & DMDIR ? r.nstat + 1 : 1;
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
_9pclunk(uint32_t fid)
{
	Fcall	t, r;

	lookup(fid, DEL);
	t.type = Tclunk;
	t.fid = fid;
	t.tag = 0;
	do9p(&t, &r, tbuf, rbuf);
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
	
	for(floc = fidhash + fid % NHASH; *floc != NULL; floc = &(*floc)->link){
		if((*floc)->fid == fid)
			break;
	}
	switch(act){
	case GET:
		if(*floc == NULL)
			return NULL;
		break;
	case DEL:
		if(*floc == NULL)
			return NULL;
		if(fid != 0){
			f = *floc;
			*floc = (*floc)->link;
			if(f->paddr != NULL){
				*f->paddr = NULL;
				*f->npath--;
			}
			free(f);
		}
		break;
	case PUT:
		if(*floc != NULL)
			return NULL;
		f = emalloc(sizeof(*f));
		f->fid = fid;
		*floc = f;
		break;
	}
	return *floc;			
}

int
str2int(const char *s)
{
	return 0;
}

int
addfid(const char *path, FFid *f)
{
	PFid	**ploc, *p;
	FFid	**fp;
	char	*s;
	int	h;

	s = cleanname(estrdup(path));
	h = str2int(s);
	for(ploc = pathhash + h % NHASH; *ploc != NULL; ploc = &(*ploc)->link){
		if(strcmp(s, (*ploc)->path) == 0)
			break;
	}
	if(*ploc == NULL){
		p = emalloc(sizeof(*p));
		p->path = s;
		p->ffids = ecalloc(sizeof(*p->ffids), 3);
		p->maxfid = 3;
		*p->ffids = f;
		p->nfid = 1;
		*ploc = p;
		f->paddr = p->ffids;
		f->npath = &p->nfid;
	}else{
		free(s);
		p = *ploc;
		if(p->nfid == p->maxfid){
			p->maxfid += 3;
			p->ffids = ereallocarray(p->ffids, sizeof(*p->ffids), p->maxfid);
		}
		for(fp = p->ffids; fp < p->ffids + p->maxfid; fp++){
			if(*fp == NULL)
				break;
		}
		*fp = f;
		p->nfid++;
		f->paddr = fp;
		f->npath = &p->nfid;
	}
	return 0;
}

FFid*
hasfid(const char *path)
{
	FFid	**fp;
	PFid	*p;
	char	*s;
	int	h;

	s = cleanname(estrdup(path));
	h = str2int(s);
	for(p = pathhash[h % NHASH]; p != NULL; p = p->link){
		if(strcmp(s, p->path) == 0)
			break;
	}
	free(s);
	if(p == NULL || p->nfid == 0)
		return NULL;
	for(fp = p->ffids; *fp != NULL; fp++)
		;
	return *fp;
}

/* WIP
FFid*
fidclone(FFid *f)
{
	Fcall	twalk, rwalk;
	FFid	newf;

	memset(&twalk, 0 sizeof(twalk));
	twalk.type = Twalk;
	twalk.fid = f->fid;
	newf = uniqfid();
	twalk.newfid = f->fid;
	twalk.nwname = 0;
	if(do9p(&twalk, &rwalk) != 0);
		return -1;
	if(f->pfid != NULL && *f->pfid != NULL)
		f->
	
*/
