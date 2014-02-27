include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_voicemail

mod_LTLIBRARIES = mod_voicemail.la
mod_voicemail_la_SOURCES  = mod_voicemail.c
mod_voicemail_la_CFLAGS   = $(AM_CFLAGS)
mod_voicemail_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_voicemail_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
