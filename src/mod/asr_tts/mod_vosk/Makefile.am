include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vosk

if HAVE_KS

mod_LTLIBRARIES = mod_vosk.la
mod_vosk_la_SOURCES  = mod_vosk.c
mod_vosk_la_CFLAGS   = $(AM_CFLAGS) $(KS_CFLAGS)
mod_vosk_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(KS_LIBS)
mod_vosk_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

endif
