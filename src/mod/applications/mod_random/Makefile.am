include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_random

mod_LTLIBRARIES = mod_random.la
mod_random_la_SOURCES  = mod_random.c
mod_random_la_CFLAGS   = $(AM_CFLAGS)
mod_random_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_random_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
