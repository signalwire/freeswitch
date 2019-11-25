include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_test

noinst_LTLIBRARIES = libtestmod.la
libtestmod_la_SOURCES  = mod_test.c

mod_LTLIBRARIES = mod_test.la
mod_test_la_CFLAGS   = $(AM_CFLAGS)
mod_test_la_SOURCES =
mod_test_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA) libtestmod.la
mod_test_la_LDFLAGS = -avoid-version -module -no-undefined -shared

SUBDIRS=. test

