include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_ios_push

mod_LTLIBRARIES = mod_ios_push.la
mod_ios_push_la_SOURCES  = mod_ios_push.c
mod_ios_push_la_CFLAGS   = $(AM_CFLAGS)
mod_ios_push_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_ios_push_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
