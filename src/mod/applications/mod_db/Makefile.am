include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_db

mod_LTLIBRARIES = mod_db.la
mod_db_la_SOURCES  = mod_db.c
mod_db_la_CFLAGS   = $(AM_CFLAGS)
mod_db_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_db_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
