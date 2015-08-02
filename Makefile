TARG=	9pfs
OBJS=	9pfs.o\
		9p.o\
		util.o
LIB=lib/strecpy.o\
		lib/convD2M.o\
		lib/convM2D.o\
		lib/convM2S.o\
		lib/convS2M.o\
		lib/read9pmsg.o\
		lib/readn.o\
		lib/cleanname.o
CC=	cc
DEBUG=	-g
CFLAGS=	-O2 -pipe\
		${DEBUG} -Wall\
		-D_FILE_OFFSET_BITS=64\
		-DFUSE_USE_VERSION=26
LDADD=	-lfuse

all:	${TARG}

${TARG}:	${OBJS} ${LIB}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LIB} ${LDADD}

9ptest: 	9ptest.o ${LIB}
	${CC} ${LDFLAGS} -o $@ 9ptest.o ${LIB} ${LDADD}
	
.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${OBJS} ${TARG} ${LIB} simplefuse 9ptest
