include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_hu

mod_LTLIBRARIES = mod_say_hu.la
mod_say_hu_la_SOURCES  = mod_say_hu.c
mod_say_hu_la_CFLAGS   = $(AM_CFLAGS)
mod_say_hu_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_hu_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
