CFLAGS += -Wimplicit -Wall -pedantic-errors --std=gnu99 -D_LARGE_FILES -D_FILE_OFFSET_BITS=64
LDFLAGS += -lmad

EXENAME = vbrinfo
VERSION = 0.2
PREFIX = /usr
BINDIR = $(PREFIX)/bin

all: $(EXENAME)

clean:
	-rm -f $(EXENAME)

install: all
	strip      $(EXENAME)
	install -d              $(DESTDIR)/$(BINDIR)
	install -c $(EXENAME)   $(DESTDIR)/$(BINDIR)
