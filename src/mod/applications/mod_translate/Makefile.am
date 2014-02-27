include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_translate

mod_LTLIBRARIES = mod_translate.la
mod_translate_la_SOURCES  = mod_translate.c
mod_translate_la_CFLAGS   = $(AM_CFLAGS)
mod_translate_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_translate_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
