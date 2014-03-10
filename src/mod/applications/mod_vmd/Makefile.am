include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vmd

mod_LTLIBRARIES = mod_vmd.la
mod_vmd_la_SOURCES  = mod_vmd.c
mod_vmd_la_CFLAGS   = $(AM_CFLAGS)
mod_vmd_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_vmd_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
