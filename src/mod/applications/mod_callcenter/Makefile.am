include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_callcenter

mod_LTLIBRARIES = mod_callcenter.la
mod_callcenter_la_SOURCES  = mod_callcenter.c
mod_callcenter_la_CFLAGS   = $(AM_CFLAGS)
mod_callcenter_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_callcenter_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
