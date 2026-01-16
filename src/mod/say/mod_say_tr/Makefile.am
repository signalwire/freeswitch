include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_tr

mod_LTLIBRARIES = mod_say_tr.la
mod_say_tr_la_SOURCES  = mod_say_tr.c
mod_say_tr_la_CFLAGS   = $(AM_CFLAGS)
mod_say_tr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_tr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
