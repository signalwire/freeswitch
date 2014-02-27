include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_reference

mod_LTLIBRARIES = mod_reference.la
mod_reference_la_SOURCES  = mod_reference.c
mod_reference_la_CFLAGS   = $(AM_CFLAGS)
mod_reference_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_reference_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
