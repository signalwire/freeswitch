include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vlc

mod_LTLIBRARIES = mod_vlc.la
mod_vlc_la_SOURCES  = mod_vlc.c
mod_vlc_la_CFLAGS   = $(AM_CFLAGS)
mod_vlc_la_LIBADD   = $(switch_builddir)/libfreeswitch.la 
mod_vlc_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lvlc
