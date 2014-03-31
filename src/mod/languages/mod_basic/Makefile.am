include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_basic

mod_LTLIBRARIES = mod_basic.la
mod_basic_la_SOURCES  = mod_basic.c my_basic.c
mod_basic_la_CFLAGS   = $(AM_CFLAGS)
mod_basic_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_basic_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
