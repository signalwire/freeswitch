include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_av

if HAVE_AVFORMAT

if HAVE_SWRESAMPLE
RESAMPLE_CFLAGS = $(SWRESAMPLE_CFLAGS)
RESAMPLE_LIBS = $(SWRESAMPLE_LIBS)
else
RESAMPLE_CFLAGS = $(AVRESAMPLE_CFLAGS) -DUSE_AVRESAMPLE
RESAMPLE_LIBS = $(AVRESAMPLE_LIBS)
endif

noinst_LTLIBRARIES = libavmod.la

libavmod_la_SOURCES  = mod_av.c avformat.c avcodec.c
libavmod_la_CFLAGS   = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVCODEC_CFLAGS) $(SWSCALE_CFLAGS) $(AVUTIL_CFLAGS) $(RESAMPLE_CFLAGS)

mod_LTLIBRARIES = mod_av.la
mod_av_la_SOURCES  =
mod_av_la_LIBADD   = libavmod.la $(switch_builddir)/libfreeswitch.la $(AVFORMAT_LIBS) $(AVCODEC_LIBS) $(SWSCALE_LIBS)  $(AVUTIL_LIBS) $(RESAMPLE_LIBS)
mod_av_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz


noinst_PROGRAMS = test/test_mod_av test/test_avformat
test_test_mod_av_CFLAGS = $(SWITCH_AM_CFLAGS) -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\" $(AVFORMAT_CFLAGS) $(AVCODEC_CFLAGS) $(SWSCALE_CFLAGS) $(AVUTIL_CFLAGS) $(RESAMPLE_CFLAGS)
test_test_mod_av_LDFLAGS = $(AVFORMAT_LIBS) $(AVCODEC_LIBS) $(SWSCALE_LIBS)  $(AVUTIL_LIBS) $(RESAMPLE_LIBS) -avoid-version -no-undefined $(SWITCH_AM_LDFLAGS)
test_test_mod_av_LDADD = libavmod.la $(switch_builddir)/libfreeswitch.la
test_test_avformat_CFLAGS = $(SWITCH_AM_CFLAGS) -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\" $(AVFORMAT_CFLAGS) $(AVCODEC_CFLAGS) $(SWSCALE_CFLAGS) $(AVUTIL_CFLAGS) $(RESAMPLE_CFLAGS)
test_test_avformat_LDFLAGS = $(AVFORMAT_LIBS) $(AVCODEC_LIBS) $(SWSCALE_LIBS)  $(AVUTIL_LIBS) $(RESAMPLE_LIBS) -avoid-version -no-undefined $(SWITCH_AM_LDFLAGS)
test_test_avformat_LDADD = libavmod.la $(switch_builddir)/libfreeswitch.la

TESTS = $(noinst_PROGRAMS)

else
install: error
all: error
error:
	$(error You must install libavformat-dev and libswscale-dev to build mod_av)
endif
