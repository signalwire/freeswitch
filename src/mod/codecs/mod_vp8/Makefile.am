include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vp8

mod_LTLIBRARIES = mod_vp8.la
mod_vp8_la_SOURCES  = mod_vp8.c
mod_vp8_la_CFLAGS   = $(AM_CFLAGS)
mod_vp8_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_vp8_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
