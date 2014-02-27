include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_abstraction

mod_LTLIBRARIES = mod_abstraction.la
mod_abstraction_la_SOURCES  = mod_abstraction.c
mod_abstraction_la_CFLAGS   = $(AM_CFLAGS)
mod_abstraction_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_abstraction_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
