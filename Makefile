TARG=		simplefuse
CC=		cc
OBJ=		simplefuse.o
DEBUG=		-g
CFLAGS=		-O2 -pipe ${DEBUG} -Wall
LDADD=		-lfuse

${TARG}:	${OBJ}
	${CC} ${LDFLAGS} ${LDSTATIC} -o $@ ${OBJ} ${LDADD}

%.o:	%.c
	${CC} -c ${CFLAGS} $?

clean:
	rm -f *.o ${TARG}
