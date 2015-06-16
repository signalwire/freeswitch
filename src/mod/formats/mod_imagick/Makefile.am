include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_imagick

if HAVE_YUV
if HAVE_VPX
if HAVE_MAGICK

mod_LTLIBRARIES = mod_imagick.la
mod_imagick_la_SOURCES  = mod_imagick.c
mod_imagick_la_CFLAGS   = $(AM_CFLAGS) $(MAGICK_CFLAGS)
mod_imagick_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(MAGICK_LIBS)
mod_imagick_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libmagickcore-dev to build mod_imagick)
endif

else
install: error
all: error
error:
	$(error You must install libvpx2-dev to build mod_imagick)
endif


else
install: error
all: error
error:
	$(error You must install libyuv-dev to build mod_imagick)
endif

