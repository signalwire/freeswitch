include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_ps

mod_LTLIBRARIES = mod_say_ps.la
mod_say_ps_la_SOURCES  = mod_say_ps.c
mod_say_ps_la_CFLAGS   = $(AM_CFLAGS)
mod_say_ps_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_ps_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
