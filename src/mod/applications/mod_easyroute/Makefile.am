include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_easyroute

mod_LTLIBRARIES = mod_easyroute.la
mod_easyroute_la_SOURCES  = mod_easyroute.c
mod_easyroute_la_CFLAGS   = $(AM_CFLAGS)
mod_easyroute_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_easyroute_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
