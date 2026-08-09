
include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_openai_tts

mod_LTLIBRARIES = mod_openai_tts.la
mod_openai_tts_la_SOURCES  = mod_openai_tts.c utils.c
mod_openai_tts_la_CFLAGS   = $(AM_CFLAGS) -I.
mod_openai_tts_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_openai_tts_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(am_mod_openai_tts_la_OBJECTS): mod_openai_tts.h
