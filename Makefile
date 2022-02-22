BIN=/usr/local/bin
MAN=/usr/share/man/man1
TARG=9pfs
OBJS=9pfs.o\
	9p.o\
	util.o\
	lib/strecpy.o\
	lib/convD2M.o\
	lib/convM2D.o\
	lib/convM2S.o\
	lib/convS2M.o\
	lib/read9pmsg.o\
	lib/readn.o\
	lib/auth_proxy.o\
	lib/auth_rpc.o\
	lib/auth_getkey.o
HFILES=9pfs.h\
	auth.h\
	fcall.h\
	util.h\
	libc.h
CC=	cc
DEBUG=	-g
CFLAGS=	-O2 -pipe\
		${DEBUG} -Wall\
		-D_FILE_OFFSET_BITS=64\
		-DFUSE_USE_VERSION=26\
		-D_GNU_SOURCE
LDFLAGS=
LDADD=	-lfuse

all:	${TARG}

install:	${TARG} ${TARG}.1
	install -s -m 555 -g bin ${TARG} ${BIN}
	install -m 444 -g bin ${TARG}.1 ${MAN}

installman:	${TARG}.1
	install -m 444 -g bin ${TARG}.1 ${MAN}

uninstall:
	rm -f ${BIN}/${TARG}
	rm -f ${MAN}/${TARG}.1

uninstallman:
	rm -f ${MAN}/${TARG}.1

${TARG}:	${OBJS} ${HFILES}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${TARG} ${OBJS}
