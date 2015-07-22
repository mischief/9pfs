#include <sys/types.h>
#include <sys/stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include "libc.h"
#include "dat.h"
#include "fcall.h"
#include "util.h"

int		srvfd;
FFid		*rootfid;
uint32_t	msize;
char		*gbuf;

void	usage(void);

void *p9init(struct fuse_conn_info*);

struct fuse_operations fsops = {
	.init = p9init,
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

	msize = 8192;
	if(!(UFLAG || TFLAG) || argc == 0){
		fprintf(stderr, "I'm here?\n");
		usage();
	}
	memset(&p9addr, 0, sizeof(p9addr));
	p9addr.sun_family = AF_UNIX;
	strecpy(p9addr.sun_path,
		p9addr.sun_path+sizeof(p9addr.sun_path),
		addr);
	srvfd = socket(p9addr.sun_family, SOCK_STREAM, 0);
	connect(srvfd, (struct sockaddr*)&p9addr, sizeof(p9addr));
	fuse_main(argc, argv, &fsops, NULL);
	exit(0);
}

int
p9version(int fd, uint32_t *m)
{
	Fcall	vcall;
	char	*buf;
	int	s;

	buf = emalloc(*m);
	vcall.type = Tversion;
	vcall.tag = (ushort)~0;
	vcall.msize = *m;
	vcall.version = VERSION9P;

	s = sizeS2M(&vcall);
	if(convS2M(&vcall, buf, s) != s)
		errx(1, "Bad Fcall conversion.");
	write(fd, buf, s);
	s = read(fd, buf, *m);
	convM2S(buf, s, &vcall);
	*m = vcall.msize;
	fprintf(stderr, "%d\n", msize);
	return s;
}

FFid*
p9attach(int fd, uint16_t fid, uint16_t afid)
{
	Fcall	acall;

	return NULL;
}	

void*
p9init(struct fuse_conn_info *f)
{
	p9version(srvfd, &msize);
	gbuf = emalloc(msize);
	rootfid = p9attach(srvfd, 0, -1);
	return NULL;
}	


void
usage(void)
{
	exit(1);
}
