include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_oreka

mod_LTLIBRARIES = mod_oreka.la
mod_oreka_la_SOURCES  = mod_oreka.c
mod_oreka_la_CFLAGS   = $(AM_CFLAGS)
mod_oreka_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_oreka_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
