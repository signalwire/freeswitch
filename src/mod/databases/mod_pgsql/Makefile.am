include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_pgsql

if HAVE_PGSQL


mod_LTLIBRARIES = mod_pgsql.la
mod_pgsql_la_SOURCES  = mod_pgsql.c
mod_pgsql_la_CFLAGS   = $(AM_CFLAGS) $(POSTGRESQL_CFLAGS)
mod_pgsql_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_pgsql_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(POSTGRESQL_LDFLAGS)

else
install: error
all: error
error:
	$(error You must install libpq-dev to build mod_pgsql)
endif
