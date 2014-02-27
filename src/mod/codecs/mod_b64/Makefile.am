include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_b64

mod_LTLIBRARIES = mod_b64.la
mod_b64_la_SOURCES  = mod_b64.c
mod_b64_la_CFLAGS   = $(AM_CFLAGS)
mod_b64_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_b64_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
