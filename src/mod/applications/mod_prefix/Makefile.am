include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_prefix

mod_LTLIBRARIES = mod_prefix.la
mod_prefix_la_SOURCES  = mod_prefix.c trie.c
mod_prefix_la_CFLAGS   = $(AM_CFLAGS)
mod_prefix_la_CPPFLAGS = -I. $(AM_CPPFLAGS)
mod_prefix_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_prefix_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
