AM_CFLAGS = -Wall -O2
AM_CFLAGS += -g -Wall -Wstrict-prototypes -I .
AM_CFLAGS += -DDEBUG_SUPPORT -DLIBIAX
AM_CFLAGS += -fsigned-char
# -DDEBUG_DEFAULT 
AM_CFLAGS += $(UCFLAGS)


pkgdir = $(libdir)
pkg_LTLIBRARIES=libiax.la
libiax_la_SOURCES = iax2-parser.c iax.c md5.c jitterbuf.c
EXTRA_DIST = md5.h frame.h iax-client.h iax2.h iax2-parser.h jitterbuf.h

install-data-local:
	mkdir -p $(includedir)/iax
	install -m 644 md5.h $(includedir)/iax
	install -m 644 frame.h $(includedir)/iax
	install -m 644 iax.h $(includedir)/iax
	install -m 644 iax2.h $(includedir)/iax
	install -m 644 iax2-parser.h $(includedir)/iax
	install -m 644 iax-client.h $(includedir)/iax

