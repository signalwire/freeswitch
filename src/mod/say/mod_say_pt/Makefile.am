include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_pt

mod_LTLIBRARIES = mod_say_pt.la
mod_say_pt_la_SOURCES  = mod_say_pt.c
mod_say_pt_la_CFLAGS   = $(AM_CFLAGS)
mod_say_pt_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_pt_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
