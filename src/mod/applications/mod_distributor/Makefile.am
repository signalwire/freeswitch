include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_distributor

mod_LTLIBRARIES = mod_distributor.la
mod_distributor_la_SOURCES  = mod_distributor.c
mod_distributor_la_CFLAGS   = $(AM_CFLAGS)
mod_distributor_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_distributor_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
