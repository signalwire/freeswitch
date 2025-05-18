
include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_google_tts

mod_LTLIBRARIES = mod_google_tts.la
mod_google_tts_la_SOURCES  = mod_google_tts.c utils.c
mod_google_tts_la_CFLAGS   = $(AM_CFLAGS) -I.
mod_google_tts_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_google_tts_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(am_mod_google_tts_la_OBJECTS): mod_google_tts.h
