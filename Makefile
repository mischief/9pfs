TARG=		simplefuse
CC=		cc
OBJ=		simplefuse.o
DEBUG=		-g
CFLAGS=		-O2 -pipe\
			${DEBUG} -Wall\
			-D_FILE_OFFSET_BITS=64\
			-DFUSE_USE_VERSION=26
LDADD=		-lfuse

${TARG}:	${OBJ}
	${CC} ${LDFLAGS} ${LDSTATIC} -o $@ ${OBJ} ${LDADD}

%.o:	%.c
	${CC} -c ${CFLAGS} $?

clean:
	rm -f *.o ${TARG}
