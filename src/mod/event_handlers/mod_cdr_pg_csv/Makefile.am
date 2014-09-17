include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cdr_pg_csv

if HAVE_PGSQL

mod_LTLIBRARIES = mod_cdr_pg_csv.la
mod_cdr_pg_csv_la_SOURCES  = mod_cdr_pg_csv.c
mod_cdr_pg_csv_la_CFLAGS   = $(AM_CFLAGS) $(SWITCH_AM_CXXFLAGS)
mod_cdr_pg_csv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cdr_pg_csv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lpq $(SWITCH_AM_LDFLAGS)

else
install: error
all: error
error:
	$(error You must configure --enable-core-pgsql-support to build this module)
endif
