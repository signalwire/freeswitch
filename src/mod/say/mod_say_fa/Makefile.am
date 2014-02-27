include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_fa

mod_LTLIBRARIES = mod_say_fa.la
mod_say_fa_la_SOURCES  = mod_say_fa.c
mod_say_fa_la_CFLAGS   = $(AM_CFLAGS)
mod_say_fa_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_fa_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
