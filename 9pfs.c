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

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"

enum
{
	MSIZE = 8192
};

FFid	*rootfid;

void	usage(void);

int
fsgetattr(const char *path, struct stat *st)
{
	FFid	*f;
	int	r;

	if((f = hasfid(path)) == NULL)
		f = _9pwalk(path);
	if(f == NULL){
		errno = ENOENT;
		return -1;
	}
	r = _9pstat(f, st);
	_9pclunk(f);
	return r;
}

int
fsfgetattr(const char *path, struct stat *st, struct fuse_file_info *ffi)
{
	return _9pstat((FFid*)ffi->fh, st);
}

int
fsrelease(const char *path, struct fuse_file_info *ffi)
{
	return _9pclunk((FFid*)ffi->fh);
}

int
fstruncate(const char *path, off_t off)
{
	FFid	*f;

	if((f = hasfid(path)) != NULL)
		f = fidclone(f);
	else
		f = _9pwalk(path);
	if(f == NULL)
		errno = ENOENT;
	f->mode = OWRITE | OTRUNC;
	if(_9popen(f, f->mode) == -1){
		_9pclunk(f);
		return -EIO;
	}
	_9pclunk(f);
	return 0;
}
	
int
fsopen(const char *path, struct fuse_file_info *ffi)
{
	FFid	*f;

	if((f = hasfid(path)) != NULL)
		f = fidclone(f);
	else
		f = _9pwalk(path);
	if(f == NULL)
		return -ENOENT;
	f->mode = ffi->flags & O_ACCMODE;
	if(ffi->flags & O_TRUNC)
		f->mode |= OTRUNC;
	if(_9popen(f, f->mode) == -1){
		_9pclunk(f);
		return -EIO;
	}
	ffi->fh = (uint64_t)f;
	return 0;
}

int
fscreate(const char *path, mode_t mode, struct fuse_file_info *ffi)
{
	FFid	*f, *d;
	char	*name, *dpath;

	if((f = hasfid(path)) != NULL)
		f = fidclone(f);
	else
		f = _9pwalk(path);
	if(f != NULL){
		if(ffi->flags | O_EXCL){
			_9pclunk(f);
			return -EEXIST;
		}
		if(_9popen(f, ffi->flags & O_ACCMODE) == -1){
			_9pclunk(f);
			return -EIO;
		}
	}else{
		dpath = cleanname(estrdup(path));
		if((name = strrchr(dpath, '/')) == dpath){
			d = rootfid;
			name++;
		}else{
			*name++ = '\0';
			if((d = hasfid(dpath)) == NULL)
				d = _9pwalk(dpath);
			if(d == NULL)
				return -EIO;
		}
		f = _9pcreate(d, name, ffi->flags & 0777, ffi->flags & O_ACCMODE);
		if(f == NULL)
			return -EIO;
		addfid(cleanname(estrdup(path)), f);
	}
	ffi->fh = (uint64_t)f;
	return 0;
}

int
fsread(const char *path, char *buf, size_t size, off_t off,
	struct fuse_file_info *ffi)
{
	FFid		*f;
	uint32_t	n, r;
	u32int		s;

	f = (FFid*)ffi->fh;
	if(f->mode & O_WRONLY)
		return -EACCES;
	f->offset = off;
	s = size;
	n = 0;
	while((r = _9pread(f, buf+n, &s)) > 0){
		n += r;
	}
	if(r < 0)
		return -EIO;
	return n;
}

int
fswrite(const char *path, const char *buf, size_t size, off_t off,
	struct fuse_file_info *ffi)
{
	FFid	*f;
	u32int	n, r;
	u32int	s;

	f = (FFid*)ffi->fh;
	if(f->mode & O_RDONLY)
		return -EACCES;
	f->offset = off;
	s = size;
	n = 0;
	while((r = _9pwrite(f, (char*)buf+n, &s)) > 0){
		n += r;
	}
	if(r < 0)
		return -EIO;
	return n;
}

int
fsopendir(const char *path, struct fuse_file_info *ffi)
{
	FFid		*f;

	if((f = hasfid(path)) == NULL)
		f = _9pwalk(path);
	else
		f = fidclone(f);
	if(f == NULL)
		return -_9perrno;
	f->mode = ffi->flags & O_ACCMODE;
	if(_9popen(f, OREAD) == -1){
		_9pclunk(f);
		return -EIO;
	}
	if(!(f->qid.type & QTDIR))
		return -ENOTDIR;
	ffi->fh = (uint64_t)f;
	return 0;
}

int
fsreaddir(const char *path, void *data, fuse_fill_dir_t ffd,
	off_t off, struct fuse_file_info *ffi)
{
	Dir		*d, *e;
	long		n;
	struct stat	s;

	ffd(data, ".", NULL, 0);
	ffd(data, "..", NULL, 0);
	n = _9pdirread((FFid*)ffi->fh, &d);
	for(e = d; e < d + n; e++){
		s.st_ino = e->qid.path;
		s.st_mode = e->mode & 0777;
		ffd(data, e->name, &s, 0);
	}
	return 0;
}

struct fuse_operations fsops = {
	.getattr =	fsgetattr,
	.fgetattr =	fsfgetattr,
	.truncate =	fstruncate,
	.open =		fsopen,
	.create =	fscreate,
	.read =		fsread,
	.write =	fswrite,
	.opendir = 	fsopendir,
	.readdir = 	fsreaddir,
	.release =	fsrelease
};

int
main(int argc, char *argv[])
{
	FFid			rfid, afid;
	struct sockaddr_un	p9addr;
	char			*s, *end, *argv0;
	int			srvfd;

	if(argc != 3)
		usage();
	argv0 = *argv++;
	argc--;
	memset(&p9addr, 0, sizeof(p9addr));
	p9addr.sun_family = AF_UNIX;
	s = p9addr.sun_path;
	end = s + sizeof(p9addr.sun_path);
	s = strecpy(s, end, "/tmp/ns.ben.:0/");
	strecpy(s, end, *argv);
	srvfd = socket(p9addr.sun_family, SOCK_STREAM, 0);
	if(connect(srvfd, (struct sockaddr*)&p9addr, sizeof(p9addr)) == -1)
		err(1, "Could not connect to %s", p9addr.sun_path);
	init9p(srvfd);
	_9pversion(MSIZE);
	memset(&rfid, 0, sizeof(rfid));
	memset(&afid, 0, sizeof(afid));
	afid.fid = NOFID;
	rootfid = _9pattach(&rfid, &afid);
	fuse_main(argc, argv, &fsops, NULL);
	exit(0);
}

void
usage(void)
{
	exit(1);
}
