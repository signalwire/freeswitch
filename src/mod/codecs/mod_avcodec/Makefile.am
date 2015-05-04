include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_avcodec

if HAVE_AVCODEC

mod_LTLIBRARIES = mod_avcodec.la
mod_avcodec_la_SOURCES  = mod_avcodec.c
mod_avcodec_la_CFLAGS   = $(AM_CFLAGS) $(AVCODEC_CFLAGS) $(AVUTIL_CFLAGS)
mod_avcodec_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(AVCODEC_LIBS)  $(AVFORMAT_LIBS) $(AVUTIL_LIBS)
mod_avcodec_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz

else
install: error
all: error
error:
	$(error You must install libavcodec-dev to build mod_avcodec)
endif

