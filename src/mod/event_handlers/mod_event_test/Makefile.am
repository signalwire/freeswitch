include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_event_test

mod_LTLIBRARIES = mod_event_test.la
mod_event_test_la_SOURCES  = mod_event_test.c
mod_event_test_la_CFLAGS   = $(AM_CFLAGS)
mod_event_test_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_event_test_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
