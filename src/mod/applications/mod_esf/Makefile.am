include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_esf

mod_LTLIBRARIES = mod_esf.la
mod_esf_la_SOURCES  = mod_esf.c
mod_esf_la_CFLAGS   = $(AM_CFLAGS)
mod_esf_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_esf_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
