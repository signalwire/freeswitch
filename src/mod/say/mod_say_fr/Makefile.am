include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_fr

mod_LTLIBRARIES = mod_say_fr.la
mod_say_fr_la_SOURCES  = mod_say_fr.c
mod_say_fr_la_CFLAGS   = $(AM_CFLAGS)
mod_say_fr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_fr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
