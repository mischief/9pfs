TARG=		9pfs
OBJS=		9pfs.o util.o lib/strecpy.o
CC=		cc
DEBUG=		-g
CFLAGS=		-O2 -pipe\
			${DEBUG} -Wall\
			-D_FILE_OFFSET_BITS=64\
			-DFUSE_USE_VERSION=26
LDADD=		-lfuse

all:	${TARG}

${TARG}:	${OBJS}
	${CC} ${LDFLAGS} -o $@ $? ${LDADD}

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${OBJS} ${TARG} simplefuse
