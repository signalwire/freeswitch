EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
NAME=dingaling
PREFIX=$(prefix)
AM_CC=gcc
AM_CFLAGS	= -Wall -Werror -I./src -I$(prefix)/include
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS = -liksemel

APR_CONFIG=$(prefix)/bin/apr-1-config
APU_CONFIG=$(prefix)/bin/apu-1-config
AM_CFLAGS  += -I$(PREFIX)/include $(shell $(APR_CONFIG) --cflags --cppflags --includes)
AM_LDFLAGS += -L$(PREFIX)/lib
AM_LDFLAGS += $(shell $(APR_CONFIG) --link-ld --libs )
AM_CFLAGS  += $(shell $(APU_CONFIG) --includes) 
AM_LDFLAGS += $(shell $(APU_CONFIG) --link-ld --libs )

lib_LTLIBRARIES		= libdingaling.la
libdingaling_la_SOURCES	= src/libdingaling.c
libdingaling_la_CFLAGS	= $(AM_CFLAGS)
libdingaling_la_LDFLAGS	= 

library_includedir	= $(prefix)/include
library_include_HEADERS = src/libdingaling.h



dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf

doxclean: 
	rm -fr docs/html/ docs/man/ docs/perlmod/ docs/xml/
