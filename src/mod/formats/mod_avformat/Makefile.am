include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_avformat

if HAVE_AVFORMAT

mod_LTLIBRARIES = mod_avformat.la
mod_avformat_la_SOURCES  = mod_avformat.c
mod_avformat_la_CFLAGS   = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVCODEC_CFLAGS) $(SWSCALE_CFLAGS)
mod_avformat_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(AVFORMAT_LIBS) $(AVCODEC_LIBS) $(SWSCALE_LIBS)
mod_avformat_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz

else
install: error
all: error
error:
	$(error You must install libavformat-dev to build mod_avformat)
endif

