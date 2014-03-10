include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_limit

mod_LTLIBRARIES = mod_limit.la
mod_limit_la_SOURCES  = mod_limit.c
mod_limit_la_CFLAGS   = $(AM_CFLAGS)
mod_limit_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_limit_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
