include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_sndfile

if HAVE_SNDFILE

noinst_LTLIBRARIES = libsndfilemod.la
libsndfilemod_la_SOURCES  = mod_sndfile.c
libsndfilemod_la_CFLAGS   = $(AM_CFLAGS) $(SNDFILE_CFLAGS)

mod_LTLIBRARIES = mod_sndfile.la
mod_sndfile_la_SOURCES  = 
mod_sndfile_la_CFLAGS   = $(AM_CFLAGS) $(SNDFILE_CFLAGS)
mod_sndfile_la_LIBADD   = libsndfilemod.la $(switch_builddir)/libfreeswitch.la $(SNDFILE_LIBS)
mod_sndfile_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

noinst_PROGRAMS = test/test_sndfile test/test_sndfile_conf

test_test_sndfile_SOURCES = test/test_sndfile.c
test_test_sndfile_CFLAGS = $(AM_CFLAGS) -I./ -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_sndfile_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_sndfile_LDADD = libsndfilemod.la

test_test_sndfile_conf_SOURCES = test/test_sndfile_conf.c
test_test_sndfile_conf_CFLAGS = $(AM_CFLAGS) -I./ -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_sndfile_conf_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_sndfile_conf_LDADD = libsndfilemod.la

TESTS = $(noinst_PROGRAMS)

else
install: error
all: error
error:
	$(error You must install libsndfile-dev to build mod_sndfile)
endif
