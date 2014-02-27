include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cluechoo

mod_LTLIBRARIES = mod_cluechoo.la
mod_cluechoo_la_SOURCES  = mod_cluechoo.c
mod_cluechoo_la_CFLAGS   = $(AM_CFLAGS)
mod_cluechoo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cluechoo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lncurses
