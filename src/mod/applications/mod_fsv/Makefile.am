include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_fsv

if HAVE_YUV
if HAVE_VPX

mod_LTLIBRARIES = mod_fsv.la
mod_fsv_la_SOURCES  = mod_fsv.c
mod_fsv_la_CFLAGS   = $(AM_CFLAGS)
mod_fsv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_fsv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libvpx2-dev to build mod_fsv)
endif


else
install: error
all: error
error:
	$(error You must install libyuv-dev to build mod_fsv)
endif


