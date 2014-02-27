include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_loopback

mod_LTLIBRARIES = mod_loopback.la
mod_loopback_la_SOURCES  = mod_loopback.c
mod_loopback_la_CFLAGS   = $(AM_CFLAGS)
mod_loopback_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_loopback_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
