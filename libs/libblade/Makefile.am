ACLOCAL_AMFLAGS=-I build
EXTRA_DIST =
SUBDIRS = . test
AUTOMAKE_OPTIONS = subdir-objects

AM_CFLAGS    += -I$(top_srcdir)/src -I$(top_srcdir)/src/include

lib_LTLIBRARIES	  = libblade.la
libblade_la_SOURCES  = src/blade.c src/blade_stack.c src/blade_peer.c src/bpcp.c
libblade_la_CFLAGS	  = $(AM_CFLAGS) $(AM_CPPFLAGS)
libblade_la_LDFLAGS  =  -version-info 0:1:0 -lncurses -lpthread -lm $(AM_LDFLAGS)

library_includedir	= $(prefix)/include
library_include_HEADERS = src/include/blade.h src/include/blade_types.h src/include/blade_stack.h src/include/blade_peer.h src/include/bpcp.h
library_include_HEADERS += test/tap.h

tests: libblade.la
	$(MAKE) -C test tests




