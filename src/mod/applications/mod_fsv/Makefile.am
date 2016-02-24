include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_fsv

mod_LTLIBRARIES = mod_fsv.la
mod_fsv_la_SOURCES  = mod_fsv.c
mod_fsv_la_CFLAGS   = $(AM_CFLAGS)
mod_fsv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_fsv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
