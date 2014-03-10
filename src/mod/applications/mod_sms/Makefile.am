include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_sms

mod_LTLIBRARIES = mod_sms.la
mod_sms_la_SOURCES  = mod_sms.c
mod_sms_la_CFLAGS   = $(AM_CFLAGS)
mod_sms_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_sms_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
