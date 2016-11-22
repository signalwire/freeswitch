include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_raven

mod_LTLIBRARIES = mod_raven.la
mod_raven_la_SOURCES  = mod_raven.c
mod_raven_la_CFLAGS   = $(AM_CFLAGS)
mod_raven_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_raven_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
