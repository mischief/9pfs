TARG=9pfs
OBJS=9p.o\
	util.o\
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

install:	${TARG}
	install -s ${TARG} ${HOME}/bin

${TARG}:	${TARG}.o ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${TARG}.o ${OBJS} ${LIB} ${LDADD}

9ptest: 	junk/9ptest.o ${OBJS}
	${CC} ${LDFLAGS} -o $@ junk/9ptest.o ${OBJS}

simplefuse:	junk/simplefuse.o
	${CC} ${LDFLAGS} -o $@ junk/simplefuse.o ${LDADD}

cleannametest:	junk/cleannametest.o lib/cleanname.o
	${CC} ${LDFLAGS} -o $@ junk/cleannametest.o lib/cleanname.o
	
.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${TARG} ${LIB} ${OBJS} *.o simplefuse 9ptest cleannametest *.core 
