include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_fsk

mod_LTLIBRARIES = mod_fsk.la
mod_fsk_la_SOURCES  = mod_fsk.c fsk.c uart.c fsk_callerid.c
mod_fsk_la_CFLAGS   = $(AM_CFLAGS)
mod_fsk_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_fsk_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
