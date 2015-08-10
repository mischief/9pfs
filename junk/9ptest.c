#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <err.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../libc.h"
#include "../fcall.h"
#include "../9pfs.h"

enum
{
	MSIZE = 512
};

int
main(int argc, char *argv[])
{
	FFid			rootfid, authfid, *tfid;
	Dir			*d, *e;
	struct sockaddr_un	p9addr;
	char			*s, *end, buf[18400];
	int			srvfd, n, size, r, msize;

	if(argc < 3)
		errx(1, "What 9p file to connect to?");
	memset(&p9addr, 0, sizeof(p9addr));
	p9addr.sun_family = AF_UNIX;
	s = p9addr.sun_path;
	end = s + sizeof(p9addr.sun_path);
	s = strecpy(s, end, "/tmp/ns.ben.:0/");
	strecpy(s, end, argv[1]);
	srvfd = socket(p9addr.sun_family, SOCK_STREAM, 0);
	if(connect(srvfd, (struct sockaddr*)&p9addr, sizeof(p9addr)) == -1)
		err(1, "Could not connect to %s", argv[1]);
	init9p(srvfd);
	msize = _9pversion(MSIZE);
	fprintf(stderr, "msize is %d\n", msize);
	memset(&rootfid, 0, sizeof(rootfid));
	memset(&authfid, 0, sizeof(authfid));
	authfid.fid = NOFID;
	rootfid = *_9pattach(&rootfid, &authfid);
	fprintf(stderr, "rootfid fid is %u\n", rootfid.fid);
	fprintf(stderr, "rootfid qid is %llu\n", rootfid.qid.path);
	tfid = _9pwalkr(&rootfid, argv[2]);
	fprintf(stderr, "Walked fid is %u\n", tfid->fid);
	_9popen(tfid, OREAD);
	fprintf(stderr, "Walked qid is %llu\n", tfid->qid.path);
	fprintf(stderr, "Walked iounit is %d\n", tfid->iounit);

	n = 0;
	size = 18400;
	while((r = _9pread(tfid, buf+n, (int*)&size)) > 0){
		fprintf(stderr, "size is now %d\n", size);
		fprintf(stderr, "and we just read %d bytes\n", r);
		n += r;
	}

	write(1, buf, n);
	_9pclunk(tfid);
	close(srvfd);
	exit(0);
}
