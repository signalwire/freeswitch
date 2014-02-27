include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_h26x

mod_LTLIBRARIES = mod_h26x.la
mod_h26x_la_SOURCES  = mod_h26x.c
mod_h26x_la_CFLAGS   = $(AM_CFLAGS)
mod_h26x_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_h26x_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
