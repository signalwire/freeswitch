AM_CFLAGS   = $(new_AM_CFLAGS)
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS  = $(new_AM_LDFLAGS)

AM_CFLAGS += -DDEBUG_SUPPORT -DLIBIAX
# -DDEBUG_DEFAULT 
AM_CFLAGS += $(UCFLAGS)

pkgdir = $(libdir)
pkg_LTLIBRARIES=libiax.la
libiax_la_SOURCES = iax2-parser.c iax.c md5.c jitterbuf.c iax-mutex.c
library_includedir = $(prefix)/include/iax
library_include_HEADERS = md5.h frame.h iax-client.h iax2.h iax2-parser.h iax.h iax-mutex.h
noinst_HEADERS = jitterbuf.h

