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
libblade_la_SOURCES     = src/blade.c src/blade_stack.c src/bpcp.c src/blade_datastore.c
libblade_la_SOURCES	+= src/blade_rpcproto.c
libblade_la_SOURCES	+= src/blade_identity.c src/blade_module.c src/blade_connection.c src/blade_module_wss.c
libblade_la_SOURCES	+= src/blade_session.c src/blade_protocol.c
libblade_la_CFLAGS	= $(AM_CFLAGS) $(AM_CPPFLAGS)
libblade_la_LDFLAGS     = -version-info 0:1:0 -lncurses -lpthread -lm -lconfig $(AM_LDFLAGS)
libblade_la_LIBADD      = libunqlite.la
library_includedir	= $(prefix)/include
library_include_HEADERS = src/include/blade.h src/include/blade_types.h src/include/blade_stack.h
library_include_HEADERS += src/include/bpcp.h src/include/blade_datastore.h src/include/blade_rpcproto.h
library_include_HEADERS += src/include/blade_identity.h src/include/blade_module.h src/include/blade_connection.h 
library_include_HEADERS += src/include/blade_session.h src/include/blade_protocol.h 
library_include_HEADERS += src/include/unqlite.h test/tap.h

tests: libblade.la
	$(MAKE) -C test tests




