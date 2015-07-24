TARG=	9pfs
OBJS=	9pfs.o\
		p9.o\
		util.o\
		lib/strecpy.o\
		lib/convD2M.o\
		lib/convM2D.o\
		lib/convM2S.o\
		lib/convS2M.o\
		lib/read9pmsg.o\
		lib/readn.o
CC=	cc
DEBUG=	-g
CFLAGS=	-O2 -pipe\
		${DEBUG} -Wall\
		-D_FILE_OFFSET_BITS=64\
		-DFUSE_USE_VERSION=26
LDADD=	-lfuse

all:	${TARG}

${TARG}:	${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${OBJS} ${TARG} simplefuse
