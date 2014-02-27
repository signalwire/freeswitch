include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_lcr

mod_LTLIBRARIES = mod_lcr.la
mod_lcr_la_SOURCES  = mod_lcr.c
mod_lcr_la_CFLAGS   = $(AM_CFLAGS)
mod_lcr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_lcr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
