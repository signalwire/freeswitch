include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_sv

mod_LTLIBRARIES = mod_say_sv.la
mod_say_sv_la_SOURCES  = mod_say_sv.c
mod_say_sv_la_CFLAGS   = $(AM_CFLAGS)
mod_say_sv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_sv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
