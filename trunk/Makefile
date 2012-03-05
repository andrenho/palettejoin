CC=gcc
CFLAGS=-O3 `pkg-config --cflags libpng12`
LIBS=`pkg-config --libs libpng12`
ifeq ($(DEBUG),yes)
LIBS+=-lefence
endif

all: palettejoin

palettejoin: palettejoin.o
	$(CC) palettejoin.o -o palettejoin $(LIBS)
ifneq ($(DEBUG),yes)
	strip palettejoin
endif

palettejoin.o: palettejoin.c
	$(CC) -c palettejoin.c $(CFLAGS)

debug:
	$(MAKE) $(MAKEFILE) CFLAGS="-O0 -Wall -Wextra -g `pkg-config --cflags libpng12`" DEBUG="yes"

clean:
	rm -rf *.o palettejoin
