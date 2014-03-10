include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_skel

mod_LTLIBRARIES = mod_skel.la
mod_skel_la_SOURCES  = mod_skel.c
mod_skel_la_CFLAGS   = $(AM_CFLAGS)
mod_skel_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_skel_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
