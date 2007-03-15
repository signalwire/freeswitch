EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
NAME=dingaling
PREFIX=$(prefix)

AM_CFLAGS   = $(new_AM_CFLAGS) -I./src -I$(prefix)/include
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS  = $(new_AM_LDFLAGS)

APR_CONFIG= $(shell which ../apr/apr-1-config || which $(prefix)/bin/apr-1-config )
APU_CONFIG= $(shell which ../apr-util/apu-1-config || which $(prefix)/bin/apu-1-config)
AM_CFLAGS  += -I$(PREFIX)/include $(shell $(APR_CONFIG) --cflags --cppflags --includes)
AM_LDFLAGS += -L$(PREFIX)/lib
AM_LDFLAGS += $(shell $(APR_CONFIG) --link-ld --libs )
AM_CFLAGS  += $(shell $(APU_CONFIG) --includes) 
AM_LDFLAGS += $(shell $(APU_CONFIG) --link-ld --libs )
AM_CFLAGS  += -I../iksemel/include


lib_LTLIBRARIES		= libdingaling.la
libdingaling_la_SOURCES	= src/libdingaling.c src/sha1.c
libdingaling_la_CFLAGS	= $(AM_CFLAGS)
libdingaling_la_LDFLAGS	= 
libdingaling_la_LIBADD  = ../iksemel/src/libiksemel.la
library_includedir	= $(prefix)/include
library_include_HEADERS = src/libdingaling.h



dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf

doxclean: 
	rm -fr docs/html/ docs/man/ docs/perlmod/ docs/xml/
