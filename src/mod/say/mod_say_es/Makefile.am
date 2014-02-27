include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_es

mod_LTLIBRARIES = mod_say_es.la
mod_say_es_la_SOURCES  = mod_say_es.c
mod_say_es_la_CFLAGS   = $(AM_CFLAGS)
mod_say_es_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_es_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
