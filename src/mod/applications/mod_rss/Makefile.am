include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_rss

mod_LTLIBRARIES = mod_rss.la
mod_rss_la_SOURCES  = mod_rss.c
mod_rss_la_CFLAGS   = $(AM_CFLAGS)
mod_rss_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_rss_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
