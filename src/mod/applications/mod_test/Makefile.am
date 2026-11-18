include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_test

noinst_LTLIBRARIES = libtestmod.la
libtestmod_la_SOURCES  = mod_test.c

mod_LTLIBRARIES = mod_test.la
mod_test_la_CFLAGS   = $(AM_CFLAGS)
mod_test_la_SOURCES =
mod_test_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA) libtestmod.la
mod_test_la_LDFLAGS = -avoid-version -module -no-undefined -shared

noinst_PROGRAMS = test/test_asr test/test_tts

test_test_asr_SOURCES = test/test_asr.c
test_test_asr_CFLAGS = $(AM_CFLAGS) -I. -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_asr_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_asr_LDADD = libtestmod.la

test_test_tts_SOURCES = test/test_tts.c
test_test_tts_CFLAGS = $(AM_CFLAGS) -I. -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_tts_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_tts_LDADD = libtestmod.la

TESTS = $(noinst_PROGRAMS)

