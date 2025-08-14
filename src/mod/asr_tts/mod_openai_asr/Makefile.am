
include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_openai_asr
mod_LTLIBRARIES = mod_openai_asr.la
mod_openai_asr_la_SOURCES  = mod_openai_asr.c utils.c curl.c
mod_openai_asr_la_CFLAGS   = $(AM_CFLAGS) -I. -Wno-pointer-arith
mod_openai_asr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_openai_asr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(am_mod_openai_asr_la_OBJECTS): mod_openai_asr.h

