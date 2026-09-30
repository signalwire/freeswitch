include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_he

mod_LTLIBRARIES = mod_say_he.la
mod_say_he_la_SOURCES  = mod_say_he.c
mod_say_he_la_CFLAGS   = $(AM_CFLAGS)
mod_say_he_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_he_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

noinst_PROGRAMS = test/test_mod_say_he

test_test_mod_say_he_SOURCES = test/test_mod_say_he.c
test_test_mod_say_he_CFLAGS = $(AM_CFLAGS) -I./ -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_mod_say_he_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)

TESTS = $(noinst_PROGRAMS)
