include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_blacklist

mod_LTLIBRARIES = mod_blacklist.la
mod_blacklist_la_SOURCES  = mod_blacklist.c
mod_blacklist_la_CFLAGS   = $(AM_CFLAGS)
mod_blacklist_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_blacklist_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
