include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_dialplan_asterisk

mod_LTLIBRARIES = mod_dialplan_asterisk.la
mod_dialplan_asterisk_la_SOURCES  = mod_dialplan_asterisk.c
mod_dialplan_asterisk_la_CFLAGS   = $(AM_CFLAGS)
mod_dialplan_asterisk_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_dialplan_asterisk_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
