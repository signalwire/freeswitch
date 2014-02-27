include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_ja

mod_LTLIBRARIES = mod_say_ja.la
mod_say_ja_la_SOURCES  = mod_say_ja.c
mod_say_ja_la_CFLAGS   = $(AM_CFLAGS)
mod_say_ja_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_ja_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
