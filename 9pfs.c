#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
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

void	usage(void);

int
fsgetattr(const char *path, struct stat *st)
{
	FFid	*f;
	int	r;

	if((f = hasfid(path)) == NULL)
		f = _9pwalk(path);
	if(f == NULL)
		return -_9perrno;
	r = _9pstat(f, st);
	_9pclunk(f);
	return r;
}

int
fsflush(const char *path, struct fuse_file_info *ffi)
{
	if(_9pclunk((FFid*)ffi->fh) != 0)
		err(1, "Could not flush");
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
	f->mode = ffi->flags & 3;
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
	.opendir = 	fsopendir,
	.readdir = 	fsreaddir
};

int
main(int argc, char *argv[])
{
	FFid			rfid, afid;
	struct sockaddr_un	p9addr;
	char			*s, *end, *argv0;
	int			srvfd;

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
