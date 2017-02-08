ACLOCAL_AMFLAGS=-I build
EXTRA_DIST =
SUBDIRS = . test
AUTOMAKE_OPTIONS = subdir-objects

AM_CFLAGS    += -I$(top_srcdir)/src -I$(top_srcdir)/src/include

noinst_LTLIBRARIES      = libunqlite.la
libunqlite_la_SOURCES   = src/unqlite.c
libunqlite_la_CFLAGS    = -DUNQLITE_ENABLE_THREADS
libunqlite_la_LIBADD    = -lpthread

lib_LTLIBRARIES	        = libblade.la
libblade_la_SOURCES     = src/blade.c src/blade_stack.c src/blade_peer.c src/blade_service.c src/bpcp.c src/blade_datastore.c
libblade_la_SOURCES	+= src/blade_message.c src/blade_rpcproto.c
libblade_la_CFLAGS	= $(AM_CFLAGS) $(AM_CPPFLAGS)
libblade_la_LDFLAGS     = -version-info 0:1:0 -lncurses -lpthread -lm -lconfig $(AM_LDFLAGS)
libblade_la_LIBADD      = libunqlite.la
library_includedir	= $(prefix)/include
library_include_HEADERS = src/include/blade.h src/include/blade_types.h src/include/blade_stack.h src/include/blade_peer.h src/include/blade_service.h
library_include_HEADERS += src/include/bpcp.h src/include/blade_datastore.h src/include/blade_message.h src/include/blade_rpcproto.h
library_include_HEADERS += src/include/unqlite.h test/tap.h

tests: libblade.la
	$(MAKE) -C test tests




