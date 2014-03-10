include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_spy

mod_LTLIBRARIES = mod_spy.la
mod_spy_la_SOURCES  = mod_spy.c
mod_spy_la_CFLAGS   = $(AM_CFLAGS)
mod_spy_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_spy_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
