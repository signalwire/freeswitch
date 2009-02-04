##
## Process this file with automake to produce Makefile.in
##

INCLUDES = -I$(top_srcdir)/include

lib_LTLIBRARIES = libiksemel.la

if DO_POSIX
posix_c = io-posix.c
endif

libiksemel_la_SOURCES = \
	ikstack.c \
	utility.c \
	iks.c \
	sax.c \
	dom.c \
	$(posix_c) \
	stream.c \
	sha.c \
	jabber.c \
	filter.c \
	md5.c \
	base64.c

libiksemel_la_LDFLAGS = -version-info 4:0:1 -no-undefined
libiksemel_la_CFLAGS = $(CFLAGS) $(LIBGNUTLS_CFLAGS)
libiksemel_la_LIBADD = $(LIBGNUTLS_LIBS)
