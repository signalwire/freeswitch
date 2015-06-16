include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_mp4v2

if HAVE_YUV
if HAVE_VPX

mod_LTLIBRARIES = mod_mp4v2.la
mod_mp4v2_la_SOURCES  = mod_mp4v2.c
mod_mp4v2_la_CFLAGS   = $(AM_CFLAGS)
mod_mp4v2_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_mp4v2_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lmp4v2

else
install: error
all: error
error:
	$(error You must install libvpx2-dev to build mod_mp4v2)
endif


else
install: error
all: error
error:
	$(error You must install libyuv-dev to build mod_mp4v2)
endif

