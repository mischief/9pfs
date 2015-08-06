TARG=9pfs
OBJS=9p.o\
	util.o\
	lib/strecpy.o\
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

${TARG}:	${TARG}.o ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${TARG}.o ${OBJS} ${LIB} ${LDADD}

9ptest: 	9ptest.o ${OBJS}
	${CC} ${LDFLAGS} -o $@ 9ptest.o ${OBJS}

simplefuse:	simplefuse.o
	${CC} ${LDFLAGS} -o $@ simplefuse.o ${LDADD}

cleannametest:	cleannametest.o lib/cleanname.o
	${CC} ${LDFLAGS} -o $@ cleannametest.o lib/cleanname.o
	
.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${TARG} ${LIB} ${OBJS} *.o simplefuse 9ptest cleannametest *.core 
