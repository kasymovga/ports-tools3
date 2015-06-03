PREFIX=/usr/local
CC=gcc
LD=gcc
CFLAGS=-Wall -O2 -g -DKGA_IMPORTANT_CHECK
LDFLAGS=
CFLAGS_BASE=$(CFLAGS) -I./libkga -std=c99
LDFLAGS_BASE=$(LDFLAGS)
COMP=$(CC) $(CFLAGS_BASE) -c -o
LINK=$(LD) $(LDFLAGS_BASE) -o
LIBKGA_OPTS=CC=$(CC) LD=$(LD) PTHREAD_ENABLE=n
HEADERS=$(wildcard *.h) Makefile
OBJECTS=kga_wrappers.o shell.o port.o pkg.o misc.o port_main.o pkg_main.o main_common.o

all : portng pkgng

$(OBJECTS) : %.o : %.c $(HEADERS)
	$(COMP) $@ $<

portng: main_common.o port_main.o port.o shell.o pkg.o kga_wrappers.o misc.o libkga/libkga.a
	$(LINK) $@ $^

pkgng: main_common.o pkg_main.o pkg.o kga_wrappers.o misc.o libkga/libkga.a
	$(LINK) $@ $^

clean :
	rm -f *.o portng
	make $(LIBKGA_OPTS) -C libkga clean

libkga/libkga.a: subdirs
	:

.PHONY : clean all subdirs

subdirs:
	make CFLAGS="$(CFLAGS)" $(LIBKGA_OPTS) -C libkga libkga.a
