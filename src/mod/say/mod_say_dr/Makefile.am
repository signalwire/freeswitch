include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_dr

mod_LTLIBRARIES = mod_say_dr.la
mod_say_dr_la_SOURCES  = mod_say_dr.c
mod_say_dr_la_CFLAGS   = $(AM_CFLAGS)
mod_say_dr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_dr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
