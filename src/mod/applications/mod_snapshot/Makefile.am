include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_snapshot

mod_LTLIBRARIES = mod_snapshot.la
mod_snapshot_la_SOURCES  = mod_snapshot.c
mod_snapshot_la_CFLAGS   = $(AM_CFLAGS)
mod_snapshot_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_snapshot_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
