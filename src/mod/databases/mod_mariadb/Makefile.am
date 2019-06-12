include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_mariadb

if HAVE_MARIADB

mod_LTLIBRARIES = mod_mariadb.la
mod_mariadb_la_SOURCES  = mariadb_dsn.cpp mod_mariadb.c
mod_mariadb_la_CFLAGS   = $(AM_CFLAGS) $(MARIADB_CFLAGS)
mod_mariadb_la_CXXFLAGS = -std=c++11 $(MARIADB_CFLAGS)
mod_mariadb_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(MARIADB_LIBS)
mod_mariadb_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libmariadb-dev to build mod_mariadb. MariaDB supports non-blocking operations starting with version 5.5.21. libmariadb version should be >= 3.0.9 to support MySQL 8.0)
endif
