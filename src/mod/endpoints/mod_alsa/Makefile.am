include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_alsa

mod_LTLIBRARIES = mod_alsa.la
mod_alsa_la_SOURCES  = mod_alsa.c
mod_alsa_la_CFLAGS   = $(AM_CFLAGS) -I. -D__EXTENSION__=1
mod_alsa_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_alsa_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lasound 
