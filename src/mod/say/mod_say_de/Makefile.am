include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_de

mod_LTLIBRARIES = mod_say_de.la
mod_say_de_la_SOURCES  = mod_say_de.c
mod_say_de_la_CFLAGS   = $(AM_CFLAGS)
mod_say_de_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_de_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
