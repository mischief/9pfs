#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <fcntl.h>
#include <fuse.h>
#include <pwd.h>
#include <grp.h>
#include <err.h>
#include <errno.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"
#include "auth.h"
#include "util.h"

#define CACHECTL ".fscache"

enum
{
	CACHECTLSIZE = 8, /* sizeof("cleared\n") - 1 */
	MSIZE = 8192
};

void	dir2stat(struct stat*, Dir*);
Dir	*iscached(const char*);
Dir	*addtocache(const char*);
void	clearcache(const char*);
int	iscachectl(const char*);
char	*breakpath(char*);
void	usage(void);

Dir	*rootdir;

int
fsstat(const char *path, struct stat *st)
{
	FFid	*f;
	Dir	*d;

	if((f = _9pwalk(path)) == NULL)
		return -EIO;
	if((d = _9pstat(f)) == NULL){
		_9pclunk(f);
		return -EACCES;
	}
	dir2stat(st, d);
	_9pclunk(f);
	free(d);
	return 0;
}

int
fsgetattr(const char *path, struct stat *st)
{
	Dir	*d;

	if(iscachectl(path)){
		st->st_mode = 0666 | S_IFREG;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_size = CACHECTLSIZE;
		return 0;
	}
	if((d = iscached(path)) == NULL)
		d = addtocache(path);
	if(d == NULL)
		return -ENOENT;
	if(strcmp(d->uid, "stub") == 0) /* hack for aux/stub */
		return fsstat(path, st);
	dir2stat(st, d);
	return 0;
}

int
fsrelease(const char *path, struct fuse_file_info *ffi)
{
	return _9pclunk((FFid*)ffi->fh);
}

int
fsreleasedir(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	if((FFid*)ffi->fh == NULL)
		return 0;
	f = (FFid*)ffi->fh;
	if((f->qid.type & QTDIR) == 0)
		return -ENOTDIR;
	return _9pclunk(f);
}

int
fstruncate(const char *path, off_t off)
{
	FFid	*f;
	Dir	*d;

	if(iscachectl(path))
		return 0;
	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	if(off == 0){
		f->mode = OWRITE | OTRUNC;
		if(_9popen(f) == -1){
			_9pclunk(f);
			return -EIO;
		}
	}else{
		if((d = _9pstat(f)) == NULL){
			_9pclunk(f);
			return -EIO;
		}
		d->length = off;
		if(_9pwstat(f, d) == -1){
			_9pclunk(f);
			free(d);
			return -EACCES;
		}
		free(d);
	}
	_9pclunk(f);
	clearcache(path);
	return 0;
}

int
fsrename(const char *opath, const char *npath)
{
	Dir	*d;
	FFid	*f;
	char	*dname, *bname;

	if(iscachectl(opath))
		return -EACCES;
	if((f = _9pwalk(opath)) == NULL)
		return -ENOENT;
	dname = estrdup(npath);
	bname = strrchr(dname, '/');
	if(strncmp(opath, npath, bname-dname) != 0){
		free(dname);
		return -EACCES;
	}
	*bname++ = '\0';
	if((d = _9pstat(f)) == NULL){
		free(dname);
		return -EIO;
	}
	d->name = bname;
	if(_9pwstat(f, d) == -1){
		_9pclunk(f);
		free(dname);
		free(d);
		return -EACCES;
	}
	_9pclunk(f);
	free(dname);
	free(d);
	clearcache(opath);
	return 0;
}	
	
int
fsopen(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	if(iscachectl(path))
		return 0;
	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	f->mode = ffi->flags & O_ACCMODE;
	if(ffi->flags & O_TRUNC)
		f->mode |= OTRUNC;
	if(_9popen(f) == -1){
		_9pclunk(f);
		return -EACCES;
	}
	ffi->fh = (u64int)f;
	return 0;
}

int
fscreate(const char *path, mode_t perm, struct fuse_file_info *ffi)
{
	FFid	*f;
	char	*dname, *bname;

	if(iscachectl(path))
		return -EACCES;
	if((f = _9pwalk(path)) == NULL){
		dname = estrdup(path);
		bname = breakpath(dname);
		if((f = _9pwalk(dname)) == NULL){
			free(dname);
			return -ENOENT;
		}
		f->mode = ffi->flags & O_ACCMODE;
		f = _9pcreate(f, bname, perm, 0);
		free(dname);
		if(f == NULL)
			return -EACCES;
	}else{
		if(ffi->flags | O_EXCL){
			_9pclunk(f);
			return -EEXIST;
		}
		f->mode = ffi->flags & O_ACCMODE;
		if(_9popen(f) == -1){
			_9pclunk(f);
			return -EIO;
		}
	}
	ffi->fh = (u64int)f;
	clearcache(path);
	return 0;
}

int
fsunlink(const char *path)
{
	FFid	*f;

	if(iscachectl(path))
		return 0;
	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	if(_9premove(f) == -1)
		return -EACCES;
	clearcache(path);
	return 0;
}

int
fsread(const char *path, char *buf, size_t size, off_t off,
	struct fuse_file_info *ffi)
{
	FFid	*f;
	int 	r;

	if(iscachectl(path)){
		size = CACHECTLSIZE;
		if(off >= size)
			return 0;
		memcpy(buf, "cleared\n" + off, size - off);
		clearcache(path);
		return size;
	}
	f = (FFid*)ffi->fh;
	if(f->mode & O_WRONLY)
		return -EACCES;
	f->offset = off;
	if((r = _9pread(f, buf, size)) < 0)
		return -EIO;
	return r;
}

int
fswrite(const char *path, const char *buf, size_t size, off_t off,
	struct fuse_file_info *ffi)
{
	FFid	*f;
	int	r;

	if(iscachectl(path)){
		clearcache(path);
		return size;
	}
	f = (FFid*)ffi->fh;
	if(f->mode & O_RDONLY)
		return -EACCES;
	f->offset = off;
	if((r = _9pwrite(f, (char*)buf, size)) < 0)
		return -EIO;
	clearcache(path);
	return r;
}

int
fsopendir(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;
	FDir	*d;

	if((d = lookupdir(path, GET)) != NULL){
		ffi->fh = (u64int)NULL;
		return 0;
	}
	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	f->mode = ffi->flags & O_ACCMODE;
	if(_9popen(f) == -1){
		_9pclunk(f);
		return -EACCES;
	}
	if(!(f->qid.type & QTDIR)){
		_9pclunk(f);
		return -ENOTDIR;
	}
	ffi->fh = (u64int)f;
	return 0;
}

int
fsmkdir(const char *path, mode_t perm)
{
	FFid	*f;
	char	*dname, *bname;

	if((f = _9pwalk(path)) != NULL){
		_9pclunk(f);
		return -EEXIST;
	}
	dname = estrdup(path);
	bname = breakpath(dname);
	if((f = _9pwalk(dname)) == NULL){
		free(dname);
		return -ENOENT;
	}
	if((f = _9pcreate(f, bname, perm, 1)) == NULL){
		free(dname);
		return -EACCES;
	}
	_9pclunk(f);
	free(dname);
	clearcache(path);
	return 0;
}

int
fsrmdir(const char *path)
{
	FFid	*f;

	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	if((f->qid.type & QTDIR) == 0){
		_9pclunk(f);
		return -ENOTDIR;
	}
	if(_9premove(f) == -1)
		return -EIO;
	clearcache(path);
	return 0;
}

int
fsreaddir(const char *path, void *data, fuse_fill_dir_t ffd,
	off_t off, struct fuse_file_info *ffi)
{
	FDir		*f;
	Dir		*d, *e;
	long		n;
	struct stat	s;

	ffd(data, ".", NULL, 0);
	ffd(data, "..", NULL, 0);
	ffd(data, CACHECTL, NULL, 0);
	if((f = lookupdir(path, GET)) != NULL){
		d = f->dirs;
		n = f->ndirs;
	}else{
		if((n = _9pdirread((FFid*)ffi->fh, &d)) < 0)
			return -EIO;
	}
	for(e = d; e < d+n; e++){
		s.st_ino = e->qid.path;
		s.st_mode = e->mode & 0777;
		ffd(data, e->name, &s, 0);
	}
	return 0;
}

int
fschmod(const char *path, mode_t perm)
{
	FFid	*f;
	Dir	*d;

	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	if((d = _9pstat(f)) == NULL){
		_9pclunk(f);
		return -EIO;
	}
	d->mode = perm & 0777;
	if(_9pwstat(f, d) == -1){
		_9pclunk(f);
		free(d);
		return -EACCES;
	}
	_9pclunk(f);
	free(d);
	clearcache(path);
	return 0;
}

struct fuse_operations fsops = {
	.getattr =	fsgetattr,
	.truncate =	fstruncate,
	.rename =	fsrename,
	.open =		fsopen,
	.create =	fscreate,
	.unlink =	fsunlink,
	.read =		fsread,
	.write =	fswrite,
	.opendir = 	fsopendir,
	.mkdir =	fsmkdir,
	.rmdir =	fsrmdir,
	.readdir = 	fsreaddir,
	.release =	fsrelease,
	.releasedir =	fsreleasedir,
	.chmod =	fschmod
};

int
main(int argc, char *argv[])
{
	AuthInfo		*ai;
	struct sockaddr_un	uaddr;
	struct sockaddr		*addr;
	struct addrinfo		*ainfo;
	struct passwd		*pw;
	char			logstr[100], *fusearg[argc], **fargp, port[10], user[30], *aname;
	int			ch, doauth, uflag, n, alen, e;

	fargp = fusearg;
	*fargp++ = *argv;
	aname = NULL;
	doauth = 0;
	uflag = 0;
	strecpy(port, port+sizeof(port), "564");
	if((pw = getpwuid(getuid())) == NULL)
		errx(1, "Could not get user");
	strecpy(user, user+sizeof(user), pw->pw_name);
	while((ch = getopt(argc, argv, ":dnUap:u:A:o:f")) != -1){
		switch(ch){
		case 'd':
			debug++;
			*fargp++ = "-d";
			break;
		case 'n':
			break;
		case 'U':
			uflag++;
			break;
		case 'a':
			doauth++;
			break;
		case 'f':
			*fargp++ = "-f";
			break;
		case 'p':
			strecpy(port, port+sizeof(port), optarg);
			break;
		case 'u':
			strecpy(user, user+sizeof(user), optarg);
			break;
		case 'A':
			aname = strdup(optarg);
			break;
                case 'o':
			*fargp++ = "-o";
			*fargp++ = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc != 2)
		usage();
	*fargp++ = "-s";
	*fargp++ = argv[1];
	if(debug){
		snprintf(logstr, sizeof(logstr), "/tmp/9pfs-%d.log", getpid());
		if((logfile = fopen(logstr, "w")) == NULL)
			err(1, "Could not open the log");
		setlinebuf(logfile);
	}

	if(uflag){
		uaddr.sun_family = AF_UNIX;
		n = sizeof(uaddr.sun_path);
		strecpy(uaddr.sun_path, uaddr.sun_path+n, argv[0]);
		addr = (struct sockaddr*)&uaddr;
		alen = sizeof(uaddr);
	}else{
		if((e = getaddrinfo(argv[0], port, NULL, &ainfo)) != 0)
			errx(1, "%s", gai_strerror(e));
		addr = ainfo->ai_addr;
		alen = ainfo->ai_addrlen;
	}
	srvfd = socket(addr->sa_family, SOCK_STREAM, 0);
	if(connect(srvfd, addr, alen) == -1)
		err(1, "Could not connect to 9p server");
	if(uflag == 0)
		freeaddrinfo(ainfo);

	init9p();
	msize = _9pversion(MSIZE);
	if(doauth){
		authfid = _9pauth(AUTHFID, user, NULL);
		ai = auth_proxy(authfid, auth_getkey, "proto=p9any role=client");
		if(ai == NULL)
			err(1, "Could not establish authentication");
		auth_freeAI(ai);
	}
	rootfid = _9pattach(ROOTFID, doauth ? AUTHFID : NOFID, user, aname);
	if((rootdir = _9pstat(rootfid)) == NULL)
		errx(1, "Could not stat root");
	DPRINT("About to fuse_main\n");
	fuse_main(fargp - fusearg, fusearg, &fsops, NULL);
	exit(0);
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

void
clearcache(const char *path)
{
	char	*dname;

	dname = estrdup(path);
	breakpath(dname);
	lookupdir(dname, DEL);
	free(dname);
	return;
}

Dir*
iscached(const char *path)
{
	FDir	*fd;
	Dir	*d, e;
	char	*dname, *bname;

	if(strcmp(path, "/") == 0)
		return rootdir;
	dname = estrdup(path);
	bname = breakpath(dname);
	if((fd = lookupdir(dname, GET)) == NULL){
		free(dname);
		return NULL;
	}
	e.name = bname;
	d = bsearch(&e, fd->dirs, fd->ndirs, sizeof(*fd->dirs), dircmp);
	free(dname);
	return d;
}

Dir*
addtocache(const char *path)
{
	FFid	*f;
	Dir	*d;
	char	*dname;
	long	n;

	DPRINT("addtocache %s\n", path);
	dname = estrdup(path);
	breakpath(dname);
	if((f = _9pwalk(dname)) == NULL){
		free(dname);
		return NULL;
	}
	f->mode |= O_RDONLY;
	if(_9popen(f) == -1){
		free(dname);
		return NULL;
	}
	DPRINT("addtocache about to dirread\n");
	if((n = _9pdirread(f, &d)) < 0){
		free(dname);
		return NULL;
	}
	free(dname);
	return iscached(path);
}
	
int
iscachectl(const char *path)
{
	char *s;

	s = strrchr(path, '/');
	s++;
	if(strcmp(s, CACHECTL) == 0)
		return 1;
	return 0;
}

char*
breakpath(char *dname)
{
	char	*bname;

	bname = strrchr(dname, '/');
	*bname++ = '\0';
	return bname;
}

void
usage(void)
{
	fprintf(stderr, "Usage: 9pfs [-anUfd] [-A aname] [-p port] [-u user] [-o option] service mtpt\n");
	exit(2);
}
