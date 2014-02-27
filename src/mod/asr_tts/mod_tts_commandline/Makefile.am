include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_tts_commandline

mod_LTLIBRARIES = mod_tts_commandline.la
mod_tts_commandline_la_SOURCES  = mod_tts_commandline.c
mod_tts_commandline_la_CFLAGS   = $(AM_CFLAGS)
mod_tts_commandline_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_tts_commandline_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
