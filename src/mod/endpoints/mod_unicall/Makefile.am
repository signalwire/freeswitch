include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_unicall

mod_LTLIBRARIES = mod_unicall.la
mod_unicall_la_SOURCES  = mod_unicall.c
mod_unicall_la_CFLAGS   = $(AM_CFLAGS)
mod_unicall_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_unicall_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
