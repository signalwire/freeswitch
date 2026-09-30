
include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_google_asr
mod_LTLIBRARIES = mod_google_asr.la
mod_google_asr_la_SOURCES  = mod_google_asr.c utils.c curl.c
mod_google_asr_la_CFLAGS   = $(AM_CFLAGS) -I. -Wno-pointer-arith
mod_google_asr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_google_asr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(am_mod_google_asr_la_OBJECTS): mod_google_asr.h

