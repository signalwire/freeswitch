include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_it

mod_LTLIBRARIES = mod_say_it.la
mod_say_it_la_SOURCES  = mod_say_it.c
mod_say_it_la_CFLAGS   = $(AM_CFLAGS)
mod_say_it_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_it_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
