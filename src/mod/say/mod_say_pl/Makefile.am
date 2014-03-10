include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_pl

mod_LTLIBRARIES = mod_say_pl.la
mod_say_pl_la_SOURCES  = mod_say_pl.c
mod_say_pl_la_CFLAGS   = $(AM_CFLAGS)
mod_say_pl_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_pl_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
