include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vlc

if HAVE_VLC

mod_LTLIBRARIES = mod_vlc.la
mod_vlc_la_SOURCES  = mod_vlc.c
mod_vlc_la_CFLAGS   = $(AM_CFLAGS) $(VLC_CFLAGS)
mod_vlc_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(VLC_LIBS)
mod_vlc_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libvlc-dev to build mod_vlc)
endif
