TARGS=		simplefuse 9pfs
CC=		cc
DEBUG=		-g
CFLAGS=		-O2 -pipe\
			${DEBUG} -Wall\
			-D_FILE_OFFSET_BITS=64\
			-DFUSE_USE_VERSION=26
LDADD=		-lfuse

all:	${TARGS}

${TARGS}:	$@.o
	${CC} ${LDFLAGS} -o $@ $? ${LDADD}

.c.o:
	${CC} -c ${CFLAGS} $<

clean:
	rm -f *.o simplefuse 9pfs
