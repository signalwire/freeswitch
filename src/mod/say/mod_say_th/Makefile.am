include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_th

mod_LTLIBRARIES = mod_say_th.la
mod_say_th_la_SOURCES  = mod_say_th.c
mod_say_th_la_CFLAGS   = $(AM_CFLAGS)
mod_say_th_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_th_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
