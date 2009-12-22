##
##  Makefile for PostgreSQL extension module
##

#   NOTICE: This requires GNU make as the PostgreSQL PGXS build
#   environment is based on GNU make features!
#
#   NOTICE: Usually one would just use "PGXS := $(shell pg_config
#   --pgxs)" followed by "include $(PGXS)" as the template. The problem
#   just is that this way (at least still under PostgreSQL 8.1) one
#   cannot pass the "-L../.libs -luuid" to the command which links the
#   DSO. Hence we fiddle around with the Makefiles which "PGXS" uses
#   itself ourself.

PG_CONFIG         ?= pg_config
PGXS              := $(shell $(PG_CONFIG) --pgxs)
POSTGRES          := $(shell $(PG_CONFIG) --bindir)/postgres
top_builddir      := $(dir $(PGXS))../..
include           $(top_builddir)/src/Makefile.global
    
NAME              = uuid
OBJS              = uuid.o
SO_MAJOR_VERSION  = 1
SO_MINOR_VERSION  = 0

override CPPFLAGS := -I.. $(CPPFLAGS)
SHLIB_LINK        := -L../.libs -luuid
SHLIB_LINK        += $(shell test $(shell uname -s) = FreeBSD && echo "-Wl,-Bsymbolic")
SHLIB_LINK        += $(shell test $(shell uname -s) = Darwin && echo "-bundle_loader $(POSTGRES)")
rpath             :=

all: uuid.sql all-lib

enable_shared     = yes
include           $(top_builddir)/src/Makefile.shlib

uuid.sql: uuid.sql.in
	sed -e 's;MODULE_PATHNAME;$(DESTDIR)$(pkglibdir)/uuid$(DLSUFFIX);g' <uuid.sql.in >uuid.sql

install: all
	$(mkinstalldirs) $(DESTDIR)$(pkglibdir)
	$(mkinstalldirs) $(DESTDIR)$(datadir)
	$(INSTALL_SHLIB) $(shlib) $(DESTDIR)$(pkglibdir)/uuid$(DLSUFFIX)
	$(INSTALL_DATA) uuid.sql $(DESTDIR)$(datadir)/uuid.sql

uninstall:
	-rm -f $(DESTDIR)$(pkglibdir)/uuid$(DLSUFFIX)
	-rm -f $(DESTDIR)$(datadir)/uuid.sql

clean distclean: clean-lib
	rm -f $(OBJS)
	rm -f uuid.sql

realclean: distclean

test:

