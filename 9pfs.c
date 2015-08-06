#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"

void	usage(void);

int
fsgetattr(const char *path, struct stat *st)
{
	FFid	*f;
	int	r;

	if((f = hasfid(path)) == NULL){
		f = _9pwalk(path);
		addfid(path, f);
	}
	if(f == NULL)
		return -_9perrno;
	r = _9pstat(f, st);
	_9pclunk(f);
	return r;
}

int
fsopendir(const char *path, struct fuse_file_info *ffi)
{
	FFid		*f;

	if((f = hasfid(path)) == NULL){
		f = _9pwalk(path);
		addfid(path, f);
	}else
		f = fidclone(f);
	f->mode = ffi->flags & 3;
	if(f == NULL)
		return -_9perrno;
	if(_9popen(f, OREAD) == -1)
		return -_9perrno;
	if(!(f->qid.type & QTDIR))
		return -ENOTDIR;
	ffi->fh = (uint64_t)f;
	return 0;
}

int
fsreaddir(const char *path, void *data, fuse_fill_dir_t ffd,
	off_t off, struct fuse_file_info *ffi)
{
	FFid		*dots[2], **f;
	Dir		*d, **e;
	long		n;
	struct stat	s;

	dots[0] = (FFid*)ffi->fh;
	dots[1] = _9pwalkr(f, "..");
	for(f = dots; f < dots + 2; f++){
		s.st_ino = (*f)->qid.path;
		s.st_mode = (*f)->mode & 0777;
		ffd(data, (*f)->path, &s, 0);
	}
	_9pclunk(dots[1]);
	n = _9pdirread(*dots, &d);
	for(e = &d; e < &d + n; e++){
		s.st_ino = d->qid.path;
		s.st_mode = d->mode & 0777;
		ffd(data, (*e)->name, &s, 0);
	}
	return 0;
}

struct fuse_operations fsops = {
	.getattr =	fsgetattr,
	.opendir = 	fsopendir,
	.readdir = 	fsreaddir
};

int
main(int argc, char *argv[])
{
	FFid			rfid, afid;
	struct sockaddr_un	p9addr;
	char			*s, *end, *argv0;
	int			c, srvfd;

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
		err(1, "Could not connect to %s", p9addr.sun_family);
	init9p(srvfd);
	_9pversion(8192);
	memset(&rfid, 0, sizeof(rfid));
	memset(&afid, 0, sizeof(afid));
	afid.fid = NOFID;
	rfid = *_9pattach(&rfid, &afid);
	fuse_main(argc, argv, &fsops, NULL);
	exit(0);
}

void
usage(void)
{
	exit(1);
}
