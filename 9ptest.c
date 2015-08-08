#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"

int
main(int argc, char *argv[])
{
	FFid			rootfid, authfid, *tfid, *newfid;
	struct sockaddr_un	p9addr;
	char			*s, *end, buf[1000];
	int			srvfd, n;

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
	_9pversion(8192);
	memset(&rootfid, 0, sizeof(rootfid));
	memset(&authfid, 0, sizeof(authfid));
	authfid.fid = NOFID;
	rootfid = *_9pattach(&rootfid, &authfid);
	fprintf(stderr, "rootfid fid is %d\n", rootfid.fid);
	fprintf(stderr, "rootfid qid is %d\n", rootfid.qid.path);
	tfid = fidclone(&rootfid);
	fprintf(stderr, "Cloned fid is %d\n", tfid->fid);
	fprintf(stderr, "Cloned qid is %d\n", tfid->qid.path);
	if((newfid = _9pwalkr(tfid, argv[2])) == NULL){
		close(srvfd);
		errno = _9perrno;
		err(1, "walk");
	}
	n = _9pread(newfid, buf, 1000, 0);
	write(1, buf, n);
	close(srvfd);
	exit(0);
}
