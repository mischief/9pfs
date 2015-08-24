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

enum
{
	MSIZE = 8192
};

int	debug;

void	usage(void);

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

int
fsgetattr(const char *path, struct stat *st)
{
	FFid	*f;
	Dir	*d;

	if((d = isdircached(path)) != NULL){
		dir2stat(st, d);
		return 0;
	}
	if((f = _9pwalk(path)) == NULL){
		return -ENOENT;
	}
	if((d = _9pstat(f)) == NULL){
		_9pclunk(f);
		return -EIO;
	}
	dir2stat(st, d);
	_9pclunk(f);
	free(d);
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
	}
	_9pclunk(f);
	return 0;
}

int
fsrename(const char *opath, const char *npath)
{
	Dir	*d;
	FFid	*f;
	char	*dname, *bname;
	int	n;

	if((f = _9pwalk(opath)) == NULL)
		return -ENOENT;
	dname = estrdup(npath);
	bname = strrchr(dname, '/');
	n = bname - dname;
	if(strncmp(opath, npath, n) != 0){
		free(dname);
		return -EACCES;
	}
	*bname++ = '\0';
	if((f = _9pwalk(opath)) == NULL){
		free(dname);
		return -ENOENT;
	}
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
	return 0;
}	
	
int
fsopen(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	f->mode = ffi->flags & O_ACCMODE;
	if(ffi->flags & O_TRUNC)
		f->mode |= OTRUNC;
	if(_9popen(f) == -1){
		_9pclunk(f);
		return -EIO;
	}
	ffi->fh = (u64int)f;
	return 0;
}

int
fscreate(const char *path, mode_t perm, struct fuse_file_info *ffi)
{
	FFid	*f;
	char	*dname, *bname;

	if((f = _9pwalk(path)) == NULL){
		dname = estrdup(path);
		if((bname = strrchr(dname, '/')) == dname){
			bname++;
			f = fidclone(rootfid);
		}else{
			*bname++ = '\0';
			f = _9pwalk(dname);
		}
		if(f == NULL){
			free(dname);
			return -ENOENT;
		}
		f->mode = ffi->flags & O_ACCMODE;
		f = _9pcreate(f, bname, perm, 0);
		if(f == NULL){
			free(dname);
			return -EIO;
		}
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
	return 0;
}

int
fsunlink(const char *path)
{
	FFid	*f;

	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	if(_9premove(f) == -1)
		return -EIO;
	return 0;
}

int
fsread(const char *path, char *buf, size_t size, off_t off,
	struct fuse_file_info *ffi)
{
	FFid	*f;
	int 	r;

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

	f = (FFid*)ffi->fh;
	if(f->mode & O_RDONLY)
		return -EACCES;
	f->offset = off;
	if((r = _9pwrite(f, (char*)buf, size)) < 0)
		return -EIO;
	return r;
}

int
fsopendir(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	f->mode = ffi->flags & O_ACCMODE;
	if(_9popen(f) == -1){
		_9pclunk(f);
		return -EIO;
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
	if((bname = strrchr(dname, '/')) == dname){
		bname++;
		f = fidclone(rootfid);
	}else{
		*bname++ = '\0';
		f = _9pwalk(dname);
	}
	if(f == NULL){
		free(dname);
		return -ENOENT;
	}
	f = _9pcreate(f, bname, perm, 1);
	if(f == NULL){
		free(dname);
		return -EIO;
	}
	_9pclunk(f);
	free(dname);
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
	return 0;
}

int
fsreaddir(const char *path, void *data, fuse_fill_dir_t ffd,
	off_t off, struct fuse_file_info *ffi)
{
	Dir		*d, *e;
	int		n;
	struct stat	s;

	ffd(data, ".", NULL, 0);
	ffd(data, "..", NULL, 0);
	if((n = _9pdirread((FFid*)ffi->fh, &d)) < 0)
		return -EIO;
	for(e = d; e < d + n; e++){
		s.st_ino = e->qid.path;
		s.st_mode = e->mode & 0777;
		ffd(data, e->name, &s, 0);
	}
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
	.releasedir =	fsreleasedir
};

int
main(int argc, char *argv[])
{
	FFid			rfid, afid;
	AuthInfo		*ai;
	struct sockaddr_un	uaddr;
	struct sockaddr		*addr;
	struct addrinfo		*ainfo;
	struct passwd		*pw;
	char			logstr[100], *fusearg[6], **fargp, port[10], user[30];
	int			ch, doauth, uflag, n, slen, e;

	fargp = fusearg;
	*fargp++ = *argv;
	doauth = 0;
	uflag = 0;
	strecpy(port, port+sizeof(port), "564");
	if((pw = getpwuid(getuid())) == NULL)
		errx(1, "Could not get user");
	strecpy(user, user+sizeof(user), pw->pw_name);
	while((ch = getopt(argc, argv, ":dnUap:u:")) != -1){
		switch(ch){
		case 'd':
			debug = 1;
			*fargp++ = "-d";
			break;
		case 'n':
			doauth = 0;
			break;
		case 'U':
			uflag = 1;
			break;
		case 'a':
			doauth = 1;
			break;
		case 'p':
			strecpy(port, port+sizeof(port), optarg);
			break;
		case 'u':
			strecpy(user, user+sizeof(user), optarg);
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
		slen = sizeof(uaddr);
	}else{
		if((e = getaddrinfo(argv[0], port, NULL, &ainfo)) != 0)
			errx(1, "%s", gai_strerror(e));
		addr = ainfo->ai_addr;
		slen = ainfo->ai_addrlen;
	}
	srvfd = socket(addr->sa_family, SOCK_STREAM, 0);
	if(connect(srvfd, addr, slen) == -1)
		err(1, "Could not connect to 9p server");
	if(uflag == 0)
		freeaddrinfo(ainfo);

	init9p();
	msize = _9pversion(MSIZE);
	memset(&rfid, 0, sizeof(rfid));
	memset(&afid, 0, sizeof(afid));
	if(doauth){
		authfid = _9pauth(AUTHFID, user, NULL);
		ai = auth_proxy(authfid, auth_getkey, "proto=p9any role=client");
		if(ai == NULL)
			err(1, "Could not establish authentication");
		auth_freeAI(ai);
	}
	rootfid = _9pattach(ROOTFID, doauth ? AUTHFID : NOFID, user, NULL);
	fuse_main(fargp - fusearg, fusearg, &fsops, NULL);
	exit(0);
}	

void
usage(void)
{
	fprintf(stderr, "usage: 9pfs [-anU] [-p port] [-u user] [host] [mtpt]\n");
	exit(2);
}
