include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_av

if HAVE_AVFORMAT

noinst_LTLIBRARIES = libavmod.la

libavmod_la_SOURCES  = mod_av.c avformat.c avcodec.c
libavmod_la_CFLAGS   = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVCODEC_CFLAGS) $(SWSCALE_CFLAGS) $(AVUTIL_CFLAGS) $(AVRESAMPLE_CFALGS)

mod_LTLIBRARIES = mod_av.la
mod_av_la_SOURCES  =
mod_av_la_LIBADD   = libavmod.la $(switch_builddir)/libfreeswitch.la $(AVFORMAT_LIBS) $(AVCODEC_LIBS) $(SWSCALE_LIBS)  $(AVUTIL_LIBS) $(AVRESAMPLE_LIBS)
mod_av_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz


bin_PROGRAMS = test/test_mod_av test/test_avformat
AM_CFLAGS = $(SWITCH_AM_CFLAGS) -I../ $(AVFORMAT_CFLAGS) $(AVCODEC_CFLAGS) $(SWSCALE_CFLAGS) $(AVUTIL_CFLAGS) $(AVRESAMPLE_CFALGS)
AM_LDFLAGS = $(AVFORMAT_LIBS) $(AVCODEC_LIBS) $(SWSCALE_LIBS)  $(AVUTIL_LIBS) $(AVRESAMPLE_LIBS) -avoid-version -no-undefined $(SWITCH_AM_LDFLAGS)
TESTS = $(bin_PROGRAMS)

test_test_mod_av_LDADD = libavmod.la $(switch_builddir)/libfreeswitch.la
test_test_avformat_LDADD = libavmod.la $(switch_builddir)/libfreeswitch.la

else
install: error
all: error
error:
	$(error You must install libavformat-dev to build mod_av)
endif
