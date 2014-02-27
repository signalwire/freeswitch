include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_zh

mod_LTLIBRARIES = mod_say_zh.la
mod_say_zh_la_SOURCES  = mod_say_zh.c
mod_say_zh_la_CFLAGS   = $(AM_CFLAGS)
mod_say_zh_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_zh_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
