EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS=foreign
NAME=dingaling
PREFIX=$(prefix)
TOUCH_TARGET=@if test -f "$@" ; then touch "$@" ; fi ;

AM_CFLAGS   = $(new_AM_CFLAGS) -I./src -I$(prefix)/include
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS  = $(new_AM_LDFLAGS)

AM_CFLAGS  += `../apr/apr-1-config --cflags --cppflags --includes || $(prefix)/bin/apr-1-config --cflags --cppflags --includes`
AM_CFLAGS  += `../apr-util/apu-1-config --includes || $(prefix)/bin/apu-1-config --includes`
AM_CFLAGS  += -I../iksemel/include

lib_LTLIBRARIES		= libdingaling.la
libdingaling_la_SOURCES	= src/libdingaling.c src/sha1.c
libdingaling_la_CFLAGS	= $(AM_CFLAGS)
libdingaling_la_LDFLAGS	= $(AM_LDFLAGS)
libdingaling_la_LIBADD  = ../iksemel/src/libiksemel.la ../apr/libapr-1.la ../apr-util/libaprutil-1.la
library_includedir	= $(prefix)/include
library_include_HEADERS = src/libdingaling.h

$(libdingaling_la_SOURCES): $(libdingaling_la_LIBADD)

../apr/libapr-1.la: ../apr ../apr/.update
	cd ../apr && $(MAKE)
	$(TOUCH_TARGET)

../apr-util/libaprutil-1.la: ../apr-util ../apr-util/.update
	cd ../apr-util && $(MAKE)
	$(TOUCH_TARGET)

../iksemel/src/libiksemel.la: ../iksemel ../iksemel/.update
	cd ../iksemel && $(MAKE)
	$(TOUCH_TARGET)

dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf

doxclean: 
	rm -fr docs/html/ docs/man/ docs/perlmod/ docs/xml/
