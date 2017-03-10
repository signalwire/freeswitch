include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_clearmode

mod_LTLIBRARIES = mod_clearmode.la
mod_clearmode_la_SOURCES  = mod_clearmode.c
mod_clearmode_la_CFLAGS   = $(AM_CFLAGS)
mod_clearmode_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_clearmode_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
