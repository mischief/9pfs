#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "9p.h"
#include "util.h"

void	usage(void);

int
_9pgetattr(const char *path, struct stat *st)
{
	FFid	*f;

	if((f = _9pwalk(path)) == NULL)
		return -_9perrno;
	return _9pstat(f, st);
}

struct fuse_operations fsops = {
	.getattr =	_9pgetattr
};

int
main(int argc, char *argv[])
{
	struct sockaddr_un	p9addr;
	char			*addr;
	int			c, UFLAG, TFLAG;

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
	init9p(8192);
	fuse_main(argc, argv, &fsops, NULL);
	exit(0);
}

void
usage(void)
{
	exit(1);
}
