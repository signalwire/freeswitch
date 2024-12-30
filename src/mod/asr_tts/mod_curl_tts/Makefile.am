
include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_curl_tts

mod_LTLIBRARIES = mod_curl_tts.la
mod_curl_tts_la_SOURCES  = mod_curl_tts.c utils.c curl.c
mod_curl_tts_la_CFLAGS   = $(AM_CFLAGS) -I.
mod_curl_tts_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_curl_tts_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(am_mod_curl_tts_la_OBJECTS): mod_curl_tts.h
