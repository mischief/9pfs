TARG=9pfs
DESTDIR?=
PREFIX?=/usr/local
BIN=$(DESTDIR)$(PREFIX)/bin
MAN=$(DESTDIR)$(PREFIX)/share/man/man1
CFLAGS?=-O2 -pipe -g -Wall
CFLAGS+=-D_FILE_OFFSET_BITS=64\
	-DFUSE_USE_VERSION=26\
	-D_GNU_SOURCE\
	$(shell pkg-config --cflags fuse)
LDFLAGS?=
LDFLAGS+=$(shell pkg-config --libs fuse)

OBJS=\
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
	lib/auth_getkey.o\
	$(TARG).o\

.PHONY: all default install uninstall clean

all: default

default: $(TARG)

install: $(TARG) $(TARG).1
	install -d $(BIN)
	install -m 755 $(TARG) $(BIN)
	install -d $(MAN)
	install -m 644 $(TARG).1 $(MAN)

uninstall:
	rm -f $(BIN)/$(TARG)
	rm -f $(MAN)/$(TARG).1

$(TARG): $(OBJS)
	${CC} -o $@ ${OBJS} ${LDFLAGS}

clean:
	rm -f $(TARG) $(OBJS)
