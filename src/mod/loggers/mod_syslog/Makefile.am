include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_syslog

mod_LTLIBRARIES = mod_syslog.la
mod_syslog_la_SOURCES  = mod_syslog.c
mod_syslog_la_CFLAGS   = $(AM_CFLAGS)
mod_syslog_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_syslog_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
