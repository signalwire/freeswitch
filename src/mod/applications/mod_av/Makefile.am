include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_av

if HAVE_YUV
if HAVE_VPX
if HAVE_AVFORMAT

mod_LTLIBRARIES = mod_av.la
mod_av_la_SOURCES  = mod_av.c avformat.c avcodec.c
mod_av_la_CFLAGS   = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVCODEC_CFLAGS) $(SWSCALE_CFLAGS) $(AVUTIL_CFLAGS) $(AVRESAMPLE_CFALGS)
mod_av_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(AVFORMAT_LIBS) $(AVCODEC_LIBS) $(SWSCALE_LIBS)  $(AVUTIL_LIBS) $(AVRESAMPLE_LIBS)
mod_av_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz

else
install: error
all: error
error:
	$(error You must install libavformat-dev to build mod_av)
endif

else
install: error
all: error
error:
	$(error You must install libvpx2-dev to build mod_av)
endif


else
install: error
all: error
error:
	$(error You must install libyuv-dev to build mod_av)
endif

