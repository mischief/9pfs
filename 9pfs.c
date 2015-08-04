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
	FFid		*f;
	Dir		*d, **e;
	long		n;
	struct stat	s;

	f = (FFid*)ffi->fh;
	n = _9pdirread(f, &d);
	for(e = &d; e < &d + n; e++){
		s.st_ino = d->qid.path;
		s.st_mode = d->mode & 0777;
		ffd(data, (*e)->name, &s, 0);
	}
	return 0;
}

struct fuse_operations fsops = {
	.getattr =	fsgetattr,
	.opendir = 	fsopendir
};

int
main(int argc, char *argv[])
{
	struct sockaddr_un	p9addr;
	char			*addr;
	int			c, UFLAG, TFLAG, srvfd;

	addr = NULL;
	TFLAG = UFLAG = 0;
	while((c = getopt(argc, argv, "u:t:")) != -1){
		switch(c){
		case 't':
			if(UFLAG)
				usage();
			TFLAG = 1;
			addr = optarg;
			break;
		case 'u':
			if(TFLAG)
				usage();
			UFLAG = 1;
			addr = optarg;
			break;
		}
	}
	argc -= (optind - 1);
	argv += (optind - 1);
	if(!(UFLAG || TFLAG) || argc == 0)
		usage();

	memset(&p9addr, 0, sizeof(p9addr));
	p9addr.sun_family = AF_UNIX;
	strecpy(p9addr.sun_path,
		p9addr.sun_path+sizeof(p9addr.sun_path),
		addr);
	srvfd = socket(p9addr.sun_family, SOCK_STREAM, 0);
	connect(srvfd, (struct sockaddr*)&p9addr, sizeof(p9addr));
	init9p(srvfd);
	fuse_main(argc, argv, &fsops, NULL);
	exit(0);
}

void
usage(void)
{
	exit(1);
}
