include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_dptools

mod_LTLIBRARIES = mod_dptools.la
mod_dptools_la_SOURCES  = mod_dptools.c
mod_dptools_la_CFLAGS   = $(AM_CFLAGS)
mod_dptools_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_dptools_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
