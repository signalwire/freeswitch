include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_he

mod_LTLIBRARIES = mod_say_he.la
mod_say_he_la_SOURCES  = mod_say_he.c
mod_say_he_la_CFLAGS   = $(AM_CFLAGS)
mod_say_he_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_he_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
