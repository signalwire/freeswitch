include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_tone_stream

mod_LTLIBRARIES = mod_tone_stream.la
mod_tone_stream_la_SOURCES  = mod_tone_stream.c
mod_tone_stream_la_CFLAGS   = $(AM_CFLAGS)
mod_tone_stream_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_tone_stream_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
