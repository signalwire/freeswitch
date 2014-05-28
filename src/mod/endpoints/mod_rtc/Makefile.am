include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_rtc

mod_LTLIBRARIES = mod_rtc.la
mod_rtc_la_SOURCES  = mod_rtc.c
mod_rtc_la_CFLAGS   = $(AM_CFLAGS)
mod_rtc_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_rtc_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
