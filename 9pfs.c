#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

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
#include "9pfs.h"

enum
{
	MSIZE = 8192
};

FFid	*rootfid;
int	debug;

void	usage(void);

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
		dprint("fsgetattr path %s not found\n", path);
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

	if((f = _9pwalk(path)) == NULL)
		return -ENOENT;
	f->mode = OWRITE | OTRUNC;
	if(_9popen(f) == -1){
		_9pclunk(f);
		return -EIO;
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

	dprint("fsopen on %s\n", path);
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
		dprint("fscreate with perm %o and access %o\n", perm, ffi->flags&O_ACCMODE);
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
	int	r;
	u32int	n;

	f = (FFid*)ffi->fh;
	dprint("fsread on %s with fid %u\n", path, f->fid);
	if(f->mode & O_WRONLY)
		return -EACCES;
	f->offset = off;
	n = 0;
	while((r = _9pread(f, buf+n, size)) > 0){
		dprint("In fsread loop r is %d: %*s\n", r, r, buf+n);
		size -= r;
		n += r;
	}
	if(r < 0)
		return -EIO;
	dprint("Leaving fsread, buf is: %*s\n", n, buf);
	return n;
}

int
fswrite(const char *path, const char *buf, size_t size, off_t off,
	struct fuse_file_info *ffi)
{
	FFid	*f;
	int	r;
	u32int	n;

	f = (FFid*)ffi->fh;
	dprint("fswrite with mode %u\n", f->mode & O_ACCMODE);
	if(f->mode & O_RDONLY)
		return -EACCES;
	f->offset = off;
	n = 0;
	while((r = _9pwrite(f, (char*)buf+n, size)) > 0){
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
	n = _9pdirread((FFid*)ffi->fh, &d);
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
	struct sockaddr_un	p9addr;
	char			logstr[100], *fusearg[6], **fargp;
	int			srvfd, ch;

	fargp = fusearg;
	*fargp++ = *argv;
	while((ch = getopt(argc, argv, ":d")) != -1){
		switch(ch){
		case 'd':
			debug = 1;
			*fargp++ = "-d";
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

	memset(&p9addr, 0, sizeof(p9addr));
	p9addr.sun_family = AF_UNIX;
	snprintf(p9addr.sun_path, sizeof(p9addr.sun_path), "/tmp/ns.ben.:0/%s", argv[0]);
	srvfd = socket(p9addr.sun_family, SOCK_STREAM, 0);
	if(connect(srvfd, (struct sockaddr*)&p9addr, sizeof(p9addr)) == -1)
		err(1, "Could not connect to %s", p9addr.sun_path);

	init9p(srvfd);
	_9pversion(MSIZE);
	memset(&rfid, 0, sizeof(rfid));
	memset(&afid, 0, sizeof(afid));
	afid.fid = NOFID;
	rootfid = _9pattach(&rfid, &afid);
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
