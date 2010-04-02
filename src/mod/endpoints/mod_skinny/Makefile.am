include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_skinny

mod_LTLIBRARIES = mod_skinny.la
mod_skinny_la_SOURCES  = mod_skinny.c skinny_protocol.c skinny_tables.c skinny_api.c
mod_skinny_la_CFLAGS   = $(AM_CFLAGS)
mod_skinny_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_skinny_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
