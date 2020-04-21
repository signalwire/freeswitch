include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vosk

mod_LTLIBRARIES = mod_vosk.la
mod_vosk_la_SOURCES  = mod_vosk.c
mod_vosk_la_CFLAGS   = $(AM_CFLAGS)
mod_vosk_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_vosk_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
