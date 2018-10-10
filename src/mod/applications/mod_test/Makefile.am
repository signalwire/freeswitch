include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_test

mod_LTLIBRARIES = mod_test.la
mod_test_la_SOURCES  = mod_test.c
mod_test_la_CFLAGS   = $(AM_CFLAGS)
mod_test_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_test_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

SUBDIRS=. test
