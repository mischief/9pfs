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
	FFid			rootfid, authfid, *tfid;
	struct sockaddr_un	p9addr;
	char			*s, *end, buf[1000];
	int			srvfd;

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
	if((tfid = _9pwalk(argv[2])) == NULL){
		close(srvfd);
		errno = _9perrno;
		err(1, "walk");
	}
	_9pread(tfid, buf, sizeof(buf), 0);
	printf("%s\n", buf);
	close(srvfd);
	exit(0);
}
