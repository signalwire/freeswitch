include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_opusfile

noinst_LTLIBRARIES = libopusfilemod.la

libopusfilemod_la_SOURCES  = mod_opusfile.c
libopusfilemod_la_CFLAGS   = $(AM_CFLAGS) $(freeswitch_LDFLAGS)

mod_LTLIBRARIES = mod_opusfile.la
mod_opusfile_la_SOURCES  =
mod_opusfile_la_CFLAGS   = $(AM_CFLAGS) $(freeswitch_LDFLAGS)
mod_opusfile_la_LIBADD   = libopusfilemod.la $(switch_builddir)/libfreeswitch.la
mod_opusfile_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_OPUSFILE_DECODE
libopusfilemod_la_CFLAGS += $(OPUSFILE_DECODE_CFLAGS)
mod_opusfile_la_CFLAGS += $(OPUSFILE_DECODE_CFLAGS)
mod_opusfile_la_LIBADD += $(OPUSFILE_DECODE_LIBS)

noinst_PROGRAMS = test/test_opusfile

test_test_opusfile_SOURCES = test/test_opusfile.c
test_test_opusfile_CFLAGS = $(AM_CFLAGS) -I./ -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\" $(OPUSFILE_DECODE_CFLAGS)
test_test_opusfile_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS) $(OPUSFILE_DECODE_LIBS)

if HAVE_OPUSFILE_ENCODE
test_test_opusfile_CFLAGS += $(OPUSFILE_ENCODE_CFLAGS) -DHAVE_OPUSFILE_ENCODE
test_test_opusfile_LDFLAGS += $(OPUSFILE_ENCODE_LIBS)
endif 

test_test_opusfile_LDADD = libopusfilemod.la $(switch_builddir)/libfreeswitch.la

TESTS = $(noinst_PROGRAMS)

endif

if HAVE_OPUSFILE_ENCODE
libopusfilemod_la_CFLAGS += $(OPUSFILE_ENCODE_CFLAGS) -DHAVE_OPUSFILE_ENCODE
mod_opusfile_la_CFLAGS += $(OPUSFILE_ENCODE_CFLAGS) -DHAVE_OPUSFILE_ENCODE
mod_opusfile_la_LIBADD += $(OPUSFILE_ENCODE_LIBS)
endif

