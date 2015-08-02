#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"

int
main(void)
{
	struct sockaddr_un	p9addr;
	char			*s;
	int			l;

	memset(&p9addr, 0, sizeof(p9addr));
	p9addr.sun_family = AF_UNIX;
	s = p9addr.sun_path;
	l = sizeof(p9addr.sun_path);
	strecpy(s, s + l, "/tmp/ns.ben.:0/pika");
	srvfd = socket(p9addr.sun_family, SOCK_STREAM, 0);
	connect(srvfd, (struct sockaddr*)&p9addr, sizeof(p9addr));
	exit(0);
}
