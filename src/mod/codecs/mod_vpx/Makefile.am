include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vpx

if HAVE_VPX
mod_LTLIBRARIES = mod_vpx.la
mod_vpx_la_SOURCES  = mod_vpx.c
mod_vpx_la_LIBADD   = $(switch_builddir)/libfreeswitch.la -L/usr/local/lib/
mod_vpx_la_CFLAGS   = $(VPX_CFLAGS) $(SWITCH_AM_CFLAGS)
mod_vpx_la_LDFLAGS  = $(VPX_LDFLAGS) -avoid-version -module -no-undefined -shared
else
install: error
all: error
error:
	$(error You must install your distros libvpx-dev to build mod_vpx)
endif
