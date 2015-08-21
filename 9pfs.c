#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <fcntl.h>
#include <fuse.h>
#include <err.h>
#include <errno.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "libc.h"
#include "fcall.h"
#include "auth.h"
#include "9pfs.h"
#include "util.h"

enum
{
	MSIZE = 8192
};

int	srvfd;
FFid	*rootfid;
FFid	*authfid;
int	debug;

void	usage(void);

int
fsgetattr(const char *path, struct stat *st)
{
	FFid	*f;
	Dir	*d;

	if((d = isdircached(path)) != NULL){
		dprint("fsgetattr %s is cached\n", path);
		dir2stat(st, d);
		return 0;
	}
	if((f = _9pwalk(srvfd, path)) == NULL){
		dprint("fsgetattr path %s not found\n", path);
		return -ENOENT;
	}
	if((d = _9pstat(srvfd, f)) == NULL){
		_9pclunk(srvfd, f);
		return -EIO;
	}
	dir2stat(st, d);
	_9pclunk(srvfd, f);
	free(d);
	return 0;
}

int
fsrelease(const char *path, struct fuse_file_info *ffi)
{
	return _9pclunk(srvfd, (FFid*)ffi->fh);
}

int
fsreleasedir(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	f = (FFid*)ffi->fh;
	if((f->qid.type & QTDIR) == 0)
		return -ENOTDIR;
	return _9pclunk(srvfd, f);
}

int
fstruncate(const char *path, off_t off)
{
	FFid	*f;
	Dir	*d;

	dprint("fstruncate off is %d\n", off);
	if((f = _9pwalk(srvfd, path)) == NULL)
		return -ENOENT;
	if(off == 0){
		f->mode = OWRITE | OTRUNC;
		if(_9popen(srvfd, f) == -1){
			_9pclunk(srvfd, f);
			return -EIO;
		}
	}else{
		if((d = _9pstat(srvfd, f)) == NULL){
			_9pclunk(srvfd, f);
			return -EIO;
		}
		d->length = off;
		if(_9pwstat(srvfd, f, d) == -1){
			_9pclunk(srvfd, f);
			free(d);
			return -EACCES;
		}
	}
	_9pclunk(srvfd, f);
	return 0;
}

int
fsrename(const char *opath, const char *npath)
{
	Dir	*d;
	FFid	*f;
	char	*dname, *bname;
	int	n;

	if((f = _9pwalk(srvfd, opath)) == NULL)
		return -ENOENT;
	dname = estrdup(npath);
	bname = strrchr(dname, '/');
	n = bname - dname;
	if(strncmp(opath, npath, n) != 0){
		free(dname);
		return -EACCES;
	}
	*bname++ = '\0';
	if((f = _9pwalk(srvfd, opath)) == NULL){
		free(dname);
		return -ENOENT;
	}
	if((d = _9pstat(srvfd, f)) == NULL){
		free(dname);
		return -EIO;
	}
	d->name = bname;
	if(_9pwstat(srvfd, f, d) == -1){
		_9pclunk(srvfd, f);
		free(dname);
		free(d);
		return -EACCES;
	}
	_9pclunk(srvfd, f);
	free(dname);
	free(d);
	return 0;
}	
	
int
fsopen(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	dprint("fsopen on %s\n", path);
	if((f = _9pwalk(srvfd, path)) == NULL)
		return -ENOENT;
	f->mode = ffi->flags & O_ACCMODE;
	if(ffi->flags & O_TRUNC)
		f->mode |= OTRUNC;
	if(_9popen(srvfd, f) == -1){
		_9pclunk(srvfd, f);
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

	if((f = _9pwalk(srvfd, path)) == NULL){
		dname = estrdup(path);
		if((bname = strrchr(dname, '/')) == dname){
			bname++;
			f = fidclone(srvfd, rootfid);
		}else{
			*bname++ = '\0';
			f = _9pwalk(srvfd, dname);
		}
		if(f == NULL){
			free(dname);
			return -ENOENT;
		}
		dprint("fscreate with perm %o and access %o\n", perm, ffi->flags&O_ACCMODE);
		f->mode = ffi->flags & O_ACCMODE;
		f = _9pcreate(srvfd, f, bname, perm, 0);
		if(f == NULL){
			free(dname);
			return -EIO;
		}
	}else{
		if(ffi->flags | O_EXCL){
			_9pclunk(srvfd, f);
			return -EEXIST;
		}
		f->mode = ffi->flags & O_ACCMODE;
		if(_9popen(srvfd, f) == -1){
			_9pclunk(srvfd, f);
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

	if((f = _9pwalk(srvfd, path)) == NULL)
		return -ENOENT;
	if(_9premove(srvfd, f) == -1)
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
	dprint("fsread on %s with fid %u\n", path, f->fid);
	if(f->mode & O_WRONLY)
		return -EACCES;
	f->offset = off;
	if((r = _9pread(srvfd, f, buf, size)) < 0)
		return -EIO;
	dprint("Leaving fsread, r is %d\n", r);
	return r;
}

int
fswrite(const char *path, const char *buf, size_t size, off_t off,
	struct fuse_file_info *ffi)
{
	FFid	*f;
	int	r;
	u32int	n;

	f = (FFid*)ffi->fh;
	dprint("fswrite %*s\n", size, buf);
	if(f->mode & O_RDONLY)
		return -EACCES;
	f->offset = off;
	n = 0;
	while((r = _9pwrite(srvfd, f, (char*)buf+n, size)) > 0){
		size -= r;
		n += r;
	}
	if(r < 0)
		return -EIO;
	return n;
}

int
fsopendir(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	dprint("fsopendir\n");
	if((f = _9pwalk(srvfd, path)) == NULL)
		return -ENOENT;
	f->mode = ffi->flags & O_ACCMODE;
	if(_9popen(srvfd, f) == -1){
		_9pclunk(srvfd, f);
		return -EIO;
	}
	if(!(f->qid.type & QTDIR)){
		_9pclunk(srvfd, f);
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

	if((f = _9pwalk(srvfd, path)) != NULL){
		_9pclunk(srvfd, f);
		return -EEXIST;
	}
	dname = estrdup(path);
	if((bname = strrchr(dname, '/')) == dname){
		bname++;
		f = fidclone(srvfd, rootfid);
	}else{
		*bname++ = '\0';
		f = _9pwalk(srvfd, dname);
	}
	if(f == NULL){
		free(dname);
		return -ENOENT;
	}
	f = _9pcreate(srvfd, f, bname, perm, 1);
	if(f == NULL){
		free(dname);
		return -EIO;
	}
	_9pclunk(srvfd, f);
	free(dname);
	return 0;
}

int
fsrmdir(const char *path)
{
	FFid	*f;

	if((f = _9pwalk(srvfd, path)) == NULL)
		return -ENOENT;
	if((f->qid.type & QTDIR) == 0){
		_9pclunk(srvfd, f);
		return -ENOTDIR;
	}
	if(_9premove(srvfd, f) == -1)
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
	if((n = _9pdirread(srvfd, (FFid*)ffi->fh, &d)) < 0)
		return -EIO;
	dprint("fsreaddir returned from _9pdirread ndirs is %d\n", n);
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
	char			logstr[100], *fusearg[6], **fargp, port[10];
	int			afd, ch, doauth, uflag, n, slen, e;

	fargp = fusearg;
	*fargp++ = *argv;
	doauth = 0;
	uflag = 0;
	strecpy(port, port+sizeof(port), "564");
	while((ch = getopt(argc, argv, ":dnuap:")) != -1){
		switch(ch){
		case 'd':
			debug = 1;
			*fargp++ = "-d";
			break;
		case 'n':
			doauth = 0;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'a':
			doauth = 1;
			break;
		case 'p':
			strecpy(port, port+sizeof(port), optarg);
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
	_9pversion(srvfd, MSIZE);
	memset(&rfid, 0, sizeof(rfid));
	memset(&afid, 0, sizeof(afid));
	if(doauth){
		afid.fid = AUTHFID;
		afd = fauth(srvfd, NULL);
		ai = auth_proxy(afd, auth_getkey, "proto=p9any role=client");
		if(ai == NULL)
			err(1, "Could not establish authentication");
		auth_freeAI(ai);
		close(afd);
	}else{
		afid.fid = NOFID;
	}
	rootfid = _9pattach(srvfd, &rfid, &afid);
	fuse_main(fargp - fusearg, fusearg, &fsops, NULL);
	exit(0);
}	

void
usage(void)
{
	exit(1);
}

void
dprint(char *fmt, ...)
{
	va_list	va;

	if(debug == 0)
		return;
	va_start(va, fmt);
	vfprintf(logfile, fmt, va);
	va_end(va);
	return;
}
