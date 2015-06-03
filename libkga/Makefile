PREFIX=/usr/local
LIBDIR=$(PREFIX)/lib

PTHREAD_ENABLE=y
CC=gcc
LD=gcc
CFLAGS=-Wall -O2 -g
CFLAGS_BASE=$(CFLAGS) -fPIC -std=c99 -I.
LDFLAGS=
LDFLAGS_BASE=$(LDFLAGS)
KGA_LDFLAGS+=$(LDFLAGS_BASE)
KGA_CFLAGS=
ifeq ($(PTHREAD_ENABLE),y)
KGA_LDFLAGS+= -lpthread
KGA_CFLAGS+= -D_PTHREAD_ENABLE=1
endif
KGA_LDFLAGS=$(LDFLAGS_BASE) -lpthread
COMP=$(CC) $(CFLAGS_BASE) $(KGA_CFLAGS) -c -o
LINKSO=$(LD) -shared $(LDFLAGS_BASE) -o

LINK=$(LD) $(LDFLAGS) -o

NAME=libkga
VERSION=0
TARGETS=libkga.so libkga.a

DESTDIR?=
AR=ar
ARC=$(AR) rc 

.PHONY : all

LIBKGA_OBJECTS=kga.o exception.o exceptions.o scope.o array.o string.o wstring.o
LIBKGA_HEADERS=$(wildcard kga/*.h) Makefile

all : $(TARGETS)

libkga.so : $(LIBKGA_OBJECTS)
	$(LINKSO) $@ $(KGA_LDFLAGS) $^

libkga.a : $(LIBKGA_OBJECTS)
	$(ARC) $@ $^

$(LIBKGA_OBJECTS) : %.o : %.c $(LIBKGA_HEADERS)
	$(COMP) $@ $<

.PHONY : clean install

clean:
	rm -f *.o *.so *.a

install : $(TARGETS)
	mkdir -m 755 -p $(DESTDIR)$(LIBDIR)
	mkdir -m 755 -p $(DESTDIR)$(PREFIX)/include/kga
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/lib/
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 kga/*.h $(DESTDIR)$(PREFIX)/include/kga/
