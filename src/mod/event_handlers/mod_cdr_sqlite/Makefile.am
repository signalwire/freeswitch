include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cdr_sqlite

mod_LTLIBRARIES = mod_cdr_sqlite.la
mod_cdr_sqlite_la_SOURCES  = mod_cdr_sqlite.c
mod_cdr_sqlite_la_CFLAGS   = $(AM_CFLAGS)
mod_cdr_sqlite_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cdr_sqlite_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
