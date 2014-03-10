include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_theora

mod_LTLIBRARIES = mod_theora.la
mod_theora_la_SOURCES  = mod_theora.c
mod_theora_la_CFLAGS   = $(AM_CFLAGS)
mod_theora_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_theora_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
