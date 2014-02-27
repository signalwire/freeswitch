include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_voicemail_ivr

mod_LTLIBRARIES = mod_voicemail_ivr.la
mod_voicemail_ivr_la_SOURCES  = mod_voicemail_ivr.c ivr.c utils.c config.c menu.c
mod_voicemail_ivr_la_CFLAGS   = $(AM_CFLAGS)
mod_voicemail_ivr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_voicemail_ivr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
